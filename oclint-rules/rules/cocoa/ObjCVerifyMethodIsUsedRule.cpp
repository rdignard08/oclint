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
        return "method is used";
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

    bool VisitObjCImplementationDecl(ObjCImplementationDecl *implementation) {
        
        for (auto iterator = implementation->meth_begin(); iterator != implementation->meth_end(); iterator++) {

            bool declaredPublically = false;
            for (ObjCInterfaceDecl* interface = implementation->getClassInterface(); interface; interface = interface->getSuperClass()) {
                ObjCInterfaceDecl* definition = interface->getDefinition();
                if (definition) {
                    declaredPublically = interfaceDeclaresMethod(definition, *iterator);
                    if (declaredPublically) {
                        break;
                    }
                }
            }
            
            // check if a method in this class calls this
            
            bool usedInternally = false;
            bool possibillyUsed = false;
            for (auto internalIterator = implementation->meth_begin(); internalIterator != implementation->meth_end(); internalIterator++) {
                
                if (methodsAreEqual(*internalIterator, *iterator)) { // ignore the same method
                    continue;
                }
                
                vector<Stmt*>* statements = collectMethodStatements((*internalIterator)->getBody());
                
                for (auto statementIterator : *statements) {
                    Stmt* statement = statementIterator;
                    clang::AbstractConditionalOperator::StmtClass cls = statement->getStmtClass();
                    if (cls == clang::AbstractConditionalOperator::ObjCSelectorExprClass) {
                        if (((ObjCSelectorExpr*)statement)->getSelector() == (*iterator)->getSelector()) {
                            possibillyUsed = true;
                        }
                    } else if ((*iterator)->isInstanceMethod() && cls == clang::AbstractConditionalOperator::ObjCPropertyRefExprClass) {
                        ObjCPropertyRefExpr* propertyReference = (ObjCPropertyRefExpr*)statement;
                        if ((propertyReference->isMessagingGetter() && propertyReference->getGetterSelector() == (*iterator)->getSelector()) ||
                            (propertyReference->isMessagingSetter() && propertyReference->getSetterSelector() == (*iterator)->getSelector())) {
                            if (!propertyReference->isSuperReceiver()) {
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
                                usedInternally = true;
                            }
                        }
                    } else if (cls == clang::AbstractConditionalOperator::ObjCMessageExprClass) {
                        ObjCMessageExpr* messageSend = (ObjCMessageExpr*)statement;
                        if (messageSend->getReceiverKind() != clang::ObjCMessageExpr::SuperInstance && messageSend->getReceiverKind() != clang::ObjCMessageExpr::SuperClass) {
                            if ((*iterator)->isInstanceMethod() && messageSend->isInstanceMessage() && methodsAreEqual(*iterator, messageSend->getMethodDecl())) {
                                // probably being used... TODO:
                                usedInternally = true;
                            } else if ((*iterator)->isClassMethod() && messageSend->isClassMessage() && methodsAreEqual(*iterator, messageSend->getMethodDecl())) {
                                // probably being used... TODO:
                                usedInternally = true;
                            }
                        }
                    }
                }
                delete statements;
            }
            
            if (!declaredPublically && !usedInternally) {
                cerr << "adding violation to " << (*iterator)->getNameAsString() << endl;
                if (possibillyUsed) {
                    addViolation(*iterator, this, string("The method ") + (*iterator)->getNameAsString() + " was referenced by @selector(...) but no where else");
                } else {
                    addViolation(*iterator, this, string("The method ") + (*iterator)->getNameAsString() + " was defined but not exported or referenced here");
                }
            }
            
        }
        
        return true;
    }
};


static RuleSet rules(new ObjCVerifyMethodIsUsedRule());
