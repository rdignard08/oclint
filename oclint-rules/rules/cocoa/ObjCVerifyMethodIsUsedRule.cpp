#include <clang/AST/Attr.h>

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
//        bool ret = memcmp((void*)declaration1, (void*)declaration2, sizeof(ObjCMethodDecl)) == 0;
        bool ret = declaration1->isClassMethod() == declaration2->isClassMethod() && declaration1->getSelector() == declaration2->getSelector();
        cout << "returning " << declaration1->getSelector().getAsString() << " is " << ret << " equal to " << declaration2->getSelector().getAsString() << endl;
        return ret;
    }

    bool containerDeclaresMethod(ObjCContainerDecl* interface, ObjCMethodDecl* method) {

        cout << "testing interface " << interface->getNameAsString() << " and method " << method->getSelector().getAsString() << endl;

        // check the methods
        
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
            if (methodsAreEqual(getter, method) || methodsAreEqual(setter, method)) {
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
        statements->push_back(statement);
        auto iterator = statement->child_begin();
        for (; iterator != statement->child_end(); iterator++) {
            printf("2statement %x\n", *iterator);
            helperCollectMethodStatements(*iterator, statements);
        }
    }
    
    vector<Stmt*>* collectMethodStatements(Stmt* statement) {
        vector<Stmt*>* statements = new vector<Stmt*>();
        printf("statement %x\n", statement);
        helperCollectMethodStatements(statement, statements);
        printf("statements %x %d\n", statements, statements->size());
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
                
                cout << (*internalIterator)->getSelector().getAsString() << " body is " << endl;
                body->dumpColor();
                
                vector<Stmt*>* statements = collectMethodStatements(body);
                
                cout << "statement count is " << statements->size() << endl;
                
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
                            if ((*iterator)->isInstanceMethod() && messageSend->isInstanceMessage()) {
                                // probably being used... TODO:
                                usedInternally = true;
                            } else if ((*iterator)->isClassMethod() && messageSend->isClassMessage()) {
                                // probably being used... TODO:
                                usedInternally = true;
                            }
                        }
                    }
                    statement->dumpColor();
                    cout << "Testing statement for method " << (*iterator)->getSelector().getAsString() << " used? " << usedInternally << endl;
                }
                
            }
            
            if (!declaredPublically && !usedInternally) {
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
