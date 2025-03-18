contract C {
    string constant storageLocation = "erc7201:example.main";
    function test() public pure returns (bool) {
        return erc7201("erc7201:example.main") == erc7201(storageLocation);
    }
}
// ----
// test() -> true
