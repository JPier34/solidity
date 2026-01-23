library C {
    function f() view public {
        C[1];
    }
}
// ----
// TypeError 2876: (51-55): Index access for library types and arrays of libraries are not possible.
