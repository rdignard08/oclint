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
    bool methodsAreEqual(ObjCMethodDecl* declaration1, ObjCMethodDecl* declaration2) {
        return declaration1->isClassMethod() == declaration2->isClassMethod() && declaration1->getSelector() == declaration2->getSelector();
    }

    bool containerDeclaresMethod(ObjCContainerDecl* interface, ObjCMethodDecl* method) {

        auto methodIterator = interface->meth_begin();
        for (; methodIterator != interface->meth_end(); methodIterator++) {
            if (methodsAreEqual(*methodIterator, method)) {
                return true;
            }
        }
        
        // check the properties
        auto propertyIterator = interface->prop_begin();
        for (; propertyIterator != interface->prop_end(); propertyIterator++) {
            auto getter = propertyIterator->getGetterMethodDecl();
            auto setter = propertyIterator->getSetterMethodDecl();
            if ((getter && methodsAreEqual(getter, method)) || (setter && methodsAreEqual(setter, method))) {
                return true;
            }
        }
        
        return false;
    }
    
    bool protocolDeclaresMethod(ObjCProtocolDecl* interface, ObjCMethodDecl* method) {
        bool containerCheck = containerDeclaresMethod(interface, method);
        if (containerCheck) {
            return true;
        }
        
        // check the protocols
        
        auto protocolIterator = interface->protocol_begin();
        for (; protocolIterator != interface->protocol_end(); protocolIterator++) {
            bool protocolCheck = protocolDeclaresMethod(interface, method);
            if (protocolCheck) {
                return true;
            }
        }
        
        return false;
    }

    bool interfaceDeclaresMethod(ObjCInterfaceDecl* interface, ObjCMethodDecl* method) {
        bool containerCheck = containerDeclaresMethod(interface, method);
        if (containerCheck) {
            return true;
        }
        
        // check the protocols
        auto protocolIterator = interface->protocol_begin();
        for (; protocolIterator != interface->protocol_end(); protocolIterator++) {
            bool protocolCheck = protocolDeclaresMethod(*protocolIterator, method);
            if (protocolCheck) {
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
        auto iterator = implementation->meth_begin(), iteratorEnd = implementation->meth_end();
        for (; iterator != iteratorEnd; iterator++) {
            cerr << (*iterator)->getSelector().getAsString() << " now" << endl;
            // check interfaces
        
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
            auto internalIterator = implementation->meth_begin();
            for (; internalIterator != implementation->meth_end(); internalIterator++) {
                if (methodsAreEqual(*internalIterator, *iterator)) { // ignore the same method
                    continue;
                }
                Stmt* body = (*internalIterator)->getBody();
                cerr << (*internalIterator)->getSelector().getAsString() << " body" << endl;
                vector<Stmt*>* statements = collectMethodStatements(body);
                
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
                                cerr << __LINE__ << " " << (*iterator)->getNameAsString() << " used by " << (*internalIterator)->getNameAsString() << endl;
                                usedInternally = true;
                            }
                        }
                    } else if (cls == clang::AbstractConditionalOperator::ObjCMessageExprClass) {
                        ObjCMessageExpr* messageSend = (ObjCMessageExpr*)statement;
                        if (messageSend->getReceiverKind() != clang::ObjCMessageExpr::SuperInstance && messageSend->getReceiverKind() != clang::ObjCMessageExpr::SuperClass) {
                            if ((*iterator)->isInstanceMethod() && messageSend->isInstanceMessage() && methodsAreEqual(*iterator, messageSend->getMethodDecl())) {
                                // probably being used... TODO:
                                cerr << __LINE__ << " " << (*iterator)->getNameAsString() << " used by " << (*internalIterator)->getNameAsString() << endl;
                                usedInternally = true;
                            } else if ((*iterator)->isClassMethod() && messageSend->isClassMessage() && methodsAreEqual(*iterator, messageSend->getMethodDecl())) {
                                // probably being used... TODO:
                                cerr << __LINE__ << " " << (*iterator)->getNameAsString() << " used by " << (*internalIterator)->getNameAsString() << endl;
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
            } else {
                cerr << declaredPublically << usedInternally << " not adding violation to " << (*iterator)->getNameAsString() << endl;
            }
            
        }
        
        return true;
    }
};


static RuleSet rules(new ObjCVerifyMethodIsUsedRule());
