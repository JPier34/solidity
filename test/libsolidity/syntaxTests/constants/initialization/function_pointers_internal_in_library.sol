using L for C;

library L {
    function execute(C c) pure internal {}
}

contract C {
    C constant constC = C(address(1));

    // Library internal function pointer.
    // It is important that error "Initial value for constant variable has to be compile-time constant."
    // in not generated. This means that the initializer expressions are marked as pure/constant.
    function () internal constant executeLibIntPtr = constC.execute;
    function () external constant executeLibIntCPtr = C(address(1)).execute;
}
// ----
// TypeError 7407: (425-439): Type function (contract C) pure is not implicitly convertible to expected type function (). Attached functions cannot be converted into unattached functions.
// TypeError 7407: (495-516): Type function (contract C) pure is not implicitly convertible to expected type function () external. Attached functions cannot be converted into unattached functions.
