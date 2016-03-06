#include <clang/AST/Attr.h>
#include <iostream>
#include "oclint/AbstractASTVisitorRule.h"
#include "oclint/RuleSet.h"
#include "oclint/helper/EnforceHelper.h"
#include "oclint/util/ASTUtil.h"

using namespace std;
using namespace clang;
using namespace oclint;


class ObjCVerifyMethodIsUsedRule : public AbstractASTVisitorRule<ObjCVerifyMethodIsUsedRule>
{
private:
    bool methodSelectorsAreEqual(ObjCMethodDecl* method1, ObjCMethodDecl* method2) {
        return method1->getSelector() == method2->getSelector();
    }
    
    bool methodsAreEqual(ObjCMethodDecl* method1, ObjCMethodDecl* method2) {
        return method1->isClassMethod() == method2->isClassMethod() && methodSelectorsAreEqual(method1, method2);
    }
    
    bool propertyIncludesMethod(ObjCPropertyDecl* property, ObjCMethodDecl* method) {
        if (method->isInstanceMethod()) {
            auto getter = property->getGetterMethodDecl();
            auto setter = property->getSetterMethodDecl();
            if ((getter && methodSelectorsAreEqual(getter, method)) || (setter && methodSelectorsAreEqual(setter, method))) {
                return true;
            }
        }
        return false;
    }

    /**
     Returns true if a declared method or property matches the provided method.
     */
    bool containerDeclaresMethod(ObjCContainerDecl* interface, ObjCMethodDecl* method) {

        for (auto methodIterator = interface->meth_begin(); methodIterator != interface->meth_end(); methodIterator++) {
            if (methodsAreEqual(*methodIterator, method)) {
                return true;
            }
        }
        
        for (auto property = interface->prop_begin(); property != interface->prop_end(); property++) {
            if (propertyIncludesMethod(*property, method)) {
                return true;
            }
        }
        
        return false;
    }
    
    bool protocolDeclaresMethod(ObjCProtocolDecl* interface, ObjCMethodDecl* method) {
        
        if (containerDeclaresMethod(interface, method)) {
            return true;
        }
        
        for (auto protocol = interface->protocol_begin(); protocol != interface->protocol_end(); protocol++) {
            if (protocolDeclaresMethod(*protocol, method)) {
                return true;
            }
        }
        
        return false;
    }

    bool interfaceDeclaresMethod(ObjCInterfaceDecl* interface, ObjCMethodDecl* method) {
        
        if (containerDeclaresMethod(interface, method)) {
            return true;
        }
        
        for (auto protocol = interface->all_referenced_protocol_begin(); protocol != interface->all_referenced_protocol_end(); protocol++) {
            if (protocolDeclaresMethod(*protocol, method)) {
                return true;
            }
        }
        
        return false;
    }
    
    bool categoryDeclaresMethod(ObjCCategoryDecl* interface, ObjCMethodDecl* method) {
        
        if (containerDeclaresMethod(interface, method)) {
            return true;
        }
        
        for (auto protocol = interface->protocol_begin(); protocol != interface->protocol_end(); protocol++) {
            if (protocolDeclaresMethod(*protocol, method)) {
                return true;
            }
        }
        
        return false;
    }
    
    bool interfaceHierarchyDeclaresMethod(ObjCInterfaceDecl* interface, ObjCMethodDecl* method) {
        
        for (; interface; interface = interface->getSuperClass()) {
            ObjCInterfaceDecl* definition = interface->getDefinition();
            if (definition) {
                if (interfaceDeclaresMethod(definition, method)) {
                    return true;
                }
                
                for (auto category = interface->visible_categories_begin(); category != interface->visible_categories_end(); category++) {
                    if (categoryDeclaresMethod(*category, method)) {
                        return true;
                    }
                }
            }
        }
        
        return false;
    }
    
    bool selectorExpressionMatchesMethod(ObjCSelectorExpr* statement, ObjCMethodDecl* method) {
        return statement->getSelector() == method->getSelector();
    }
    
    bool implementationCallsMethod(ObjCImplDecl* implementation, ObjCMethodDecl* method, bool* possibillyUsed) {

        for (auto internalIterator = implementation->meth_begin(); internalIterator != implementation->meth_end(); internalIterator++) {
            ObjCMethodDecl* testMethod = *internalIterator;
            
            
            if (methodsAreEqual(testMethod, method)) { // ignore the same method
                continue;
            }
            
            vector<Stmt*>* statements = collectMethodStatements(testMethod->getBody());
            
            for (Stmt* statement : *statements) {
                clang::AbstractConditionalOperator::StmtClass cls = statement->getStmtClass();
                
                if (cls == clang::AbstractConditionalOperator::ObjCSelectorExprClass) {
                    
                    if (possibillyUsed && !*possibillyUsed) { // does the caller care, and have we not set it to true yet.
                        *possibillyUsed = selectorExpressionMatchesMethod((ObjCSelectorExpr*)statement, method);
                    }
                    
                } else if (method->isInstanceMethod() && cls == clang::AbstractConditionalOperator::ObjCPropertyRefExprClass) {
                    ObjCPropertyRefExpr* property = (ObjCPropertyRefExpr*)statement;
                    
                    if ((property->isMessagingGetter() && property->getGetterSelector() == method->getSelector()) ||
                        (property->isMessagingSetter() && property->getSetterSelector() == method->getSelector())) {
                        if (!property->isSuperReceiver()) {
//                                Type* receiverType = propertyReference->getReceiverType().getTypePtrOrNull();
//                                DeclarationName implementingName = implementation->getDeclName();
//                                Type* implementingClass = implementation->getClassInterface()->getTypeForDecl();
//                                if (!receiverType) {
//                                    possibillyUsed = true;
//                                    break;
//                                } else {
//
//                                }
// probably being used... TODO:
                            delete statements;
                            return true;
                        }
                    }
                    
                } else if (cls == clang::AbstractConditionalOperator::ObjCMessageExprClass) {
                    ObjCMessageExpr* messageSend = (ObjCMessageExpr*)statement;
                    
                    if (messageSend->getReceiverKind() != clang::ObjCMessageExpr::SuperInstance && messageSend->getReceiverKind() != clang::ObjCMessageExpr::SuperClass) { // sending to super does not count as an internal reference
                        if (method->isInstanceMethod() == messageSend->isInstanceMessage() && methodsAreEqual(method, messageSend->getMethodDecl())) {
                            // probably being used... TODO:
                            delete statements;
                            return true;
                        }
                    }
                    
                }
            }
            delete statements;
        }
        
        return false;
    }
    
    void helperCollectMethodStatements(Stmt* statement, vector<Stmt*>* statements) {
        if (!statement) return;
        statements->push_back(statement);
        auto iterator = statement->child_begin();
        for (; iterator != statement->child_end(); iterator++) {
            helperCollectMethodStatements(*iterator, statements);
        }
    }
    
    vector<Stmt*>* collectMethodStatements(Stmt* statement) {
        vector<Stmt*>* statements = new vector<Stmt*>();
        helperCollectMethodStatements(statement, statements);
        return statements;
    }
    
public:
    virtual const string name() const override
    {
        return "method usage";
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
    
    bool VisitObjCMethodDecl(ObjCMethodDecl* method) {
        
        DeclContext* context = clang::AccessSpecDecl::castToDeclContext(method)->getLexicalParent();
        if (clang::ObjCImplDecl::classofKind(context->getDeclKind())) { // if the method is lexically in a category definition or class implementation
            
            bool declaredPublically = interfaceHierarchyDeclaresMethod(method->getClassInterface(), method);
            
            bool possibillyUsed = false;
            bool usedInternally = implementationCallsMethod((ObjCImplDecl*)context, method, &possibillyUsed);
            
            if (!declaredPublically && !usedInternally) {
                cerr << "adding violation to " << method->getNameAsString() << endl;
                if (possibillyUsed) {
                    addViolation(method, this, string("The method ") + method->getNameAsString() + " was referenced by @selector(...) but no where else");
                } else {
                    addViolation(method, this, string("The method ") + method->getNameAsString() + " was defined but not exported or referenced here");
                }
            }
            
        }
        
        return true;
    }

};


static RuleSet rules(new ObjCVerifyMethodIsUsedRule());
