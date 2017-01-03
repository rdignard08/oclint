SET(LIST_OF_RULES
    ObjCVerifySafeObjects
    ObjCVerifyIsEqualHash
    ObjCVerifyMethodIsUsed
    ObjCVerifyMustCallSuper
    ObjCVerifySubclassMustImplement
    ObjCVerifyProhibitedCall
    ObjCVerifyProtectedMethod
    )

BUILD_DYNAMIC_RULES("${LIST_OF_RULES}")
