contract C {
    function test() public pure returns (uint) {
        return erc7201(["x"][0]);
    }
}
// ----
// test() -> -54410875867715754317031553053437925074622998971851806468326073383238108491776
