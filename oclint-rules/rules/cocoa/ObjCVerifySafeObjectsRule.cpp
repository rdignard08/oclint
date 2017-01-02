#include <clang/AST/Attr.h>
#include <iostream>

#include "oclint/AbstractASTVisitorRule.h"
#include "oclint/RuleSet.h"
#include "oclint/helper/EnforceHelper.h"
#include "oclint/util/ASTUtil.h"

using namespace std;
using namespace clang;
using namespace oclint;

class ObjCVerifySafeObjectsRule : public AbstractASTVisitorRule<ObjCVerifySafeObjectsRule>
{
private:
    bool declIsObjectType(ObjCPropertyDecl* decl) {
        QualType canonicalType = decl->getType();
        auto type = canonicalType.getTypePtr();
        auto typeKind = type->getScalarTypeKind();
        return typeKind == DependentDecltypeType::STK_ObjCObjectPointer;
    }

    bool declIsNotSafe(ObjCPropertyDecl* decl) {
        ObjCPropertyDecl::SetterKind setterKind = decl->getSetterKind();
        return !decl->isReadOnly() && setterKind == ObjCPropertyDecl::Assign;
    }

public:
    virtual const string name() const override
    {
        return "should use weak";
    }

    virtual int priority() const override
    {
        return 1;
    }

    virtual const string category() const override
    {
        return "cocoa";
    }

    virtual unsigned int supportedLanguages() const override
    {
        return LANG_OBJC;
    }

    bool VisitObjCPropertyDecl(ObjCPropertyDecl* decl) {

        cout << decl->getNameAsString() << endl;

        // Figure out if the property type is subject to ARC (pointer types with automatic retain/release semantics)
        if (declIsObjectType(decl)) {

            cout << "it's an object!" << endl;

            // If it's an ARC managed object property, check that it is not using assign
            if (declIsNotSafe(decl)) {

                cout << "I think it's unsafe!" << endl;

                string propertyName = decl->getNameAsString();
                string message = "property object property " + propertyName + " should use strong, copy, or weak";
                addViolation(decl, this, message);
            }
        }

        return true;
    }

};

static RuleSet rules(new ObjCVerifySafeObjectsRule());
