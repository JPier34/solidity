using L for C;

library L {
    function fooLibInternal() internal {}
}

contract C {
    function fooPrivate() private {}
    function fooInternal() internal {}
    function fooPublic() public {}
    function fooExternal() external {}

    // Internal function pointers initializers.
    function () internal constant fooIntCPtr = C.fooInternal;
    function () internal constant fooPubPCPtr = C.fooPublic;

    // External function pointers initializers.
    C constant constC = C(address(1));
    function () external constant fooExtConstCPtr = constC.fooExternal;
    function () external constant fooExtCPtr = C(address(1)).fooExternal;

    // Library internal function pointer.
    function () internal constant fooLibIntPtr = L.fooLibInternal;

    // TODO: This should work too.
    // function () internal constant fooPrvPtr = fooPrivate;
    // function () internal constant fooIntPtr = fooInternal;
    // function () internal constant fooPubPtr = fooPublic;
}
