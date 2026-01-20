library L {
    function fooLibInternal() internal {}
    function fooLibPublic() public {}
    function fooLibExternal() external {}
}

contract C {
    function fooPrivate() private {}
    function fooInternal() internal {}
    function fooPublic() public {}
    function fooExternal() external {}

    function () internal fooIntPtr = C.fooInternal;
    function () internal fooPubCPtr = C.fooPublic;

    // Constant pointer initialized with non-constant pointer.q
    function () internal constant fooIntConstPtr = fooIntPtr;
    function () internal constant fooPubConstPtr = fooPubCPtr;

    // Constant pointer initialized with non-constant contract function pointer.
    C c = C(address(1));
    function () external constant fooExtConstCPtr = c.fooExternal;
    function () external constant fooExtThisConstPtr = this.fooExternal;

    // TODO:: These errors need improvement. Function call kinds are different. `DelegateCall` and `External`
    function () external constant fooLibPubPtr = L.fooLibPublic;
    function () external constant fooLibExtPtr = L.fooLibExternal;
}
// ----
// TypeError 8349: (520-529): Initial value for constant variable has to be compile-time constant.
// TypeError 8349: (582-592): Initial value for constant variable has to be compile-time constant.
// TypeError 8349: (753-766): Initial value for constant variable has to be compile-time constant.
// TypeError 8349: (823-839): Initial value for constant variable has to be compile-time constant.
// TypeError 7407: (1001-1015): Type function () is not implicitly convertible to expected type function () external. Special functions cannot be converted to function types.
// TypeError 8349: (1001-1015): Initial value for constant variable has to be compile-time constant.
// TypeError 7407: (1066-1082): Type function () is not implicitly convertible to expected type function () external. Special functions cannot be converted to function types.
// TypeError 8349: (1066-1082): Initial value for constant variable has to be compile-time constant.
