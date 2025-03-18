contract C layout at erc7201("example.main") {
    uint x;
    function test() public pure returns (bool) {
        uint firstSlot;
        assembly {
            firstSlot := x.slot
        }
        return firstSlot == erc7201("example.main");
    }
}
// ----
// test() -> true
