contract C {
    function f0() public {
        abi.decode("",  (int[0]));
    }

    function f1() public {
        abi.decode("",  (int[0][]));
    }

    function f2() public {
        abi.decode("",  (int[][0]));
    }

    function f3() public {
        abi.decode("",  (int[][0][]));
    }

    function f4() public {
        abi.decode("",  (int[][][0]));
    }

    struct S { uint t; }
    function f5() public {
        abi.decode("",  (S[][][0]));
    }

    function f6() public {
        C[0];
    }
}
// ----
// TypeError 7015: (65-71): Array with zero length specified.
// TypeError 7015: (134-140): Array with zero length specified.
// TypeError 7015: (205-213): Array with zero length specified.
// TypeError 7015: (276-284): Array with zero length specified.
// TypeError 7015: (349-359): Array with zero length specified.
// TypeError 7015: (447-455): Array with zero length specified.
// TypeError 7015: (501-505): Array with zero length specified.
