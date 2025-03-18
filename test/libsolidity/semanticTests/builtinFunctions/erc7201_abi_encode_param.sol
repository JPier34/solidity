contract C {
    function test() public pure returns (uint) {
        return erc7201(string(abi.encode(erc7201("x"))));
    }
}
// ----
// test() -> 27977673520846093514787598001774779176730013618425076910399264148834742199808
