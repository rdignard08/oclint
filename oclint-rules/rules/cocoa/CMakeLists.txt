SET(LIST_OF_RULES
    ObjCVerifySafeObjects
    ObjCVerifyIsEqualHash
    ObjCVerifyMethodIsUsed
    ObjCVerifyDeclarationIsUsed
    ObjCVerifyMustCallSuper
    ObjCVerifySubclassMustImplement
    ObjCVerifyProhibitedCall
    ObjCVerifyProtectedMethod
    )

BUILD_DYNAMIC_RULES("${LIST_OF_RULES}")
