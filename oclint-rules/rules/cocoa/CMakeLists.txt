SET(LIST_OF_RULES
    ObjCVerifyIsEqualHash
    ObjCVerifyMethodIsUsed
    ObjCVerifyDeclarationIsUsed
    ObjCVerifyMustCallSuper
    ObjCVerifySubclassMustImplement
    ObjCVerifyProhibitedCall
    ObjCVerifyProtectedMethod
    )

BUILD_DYNAMIC_RULES("${LIST_OF_RULES}")
