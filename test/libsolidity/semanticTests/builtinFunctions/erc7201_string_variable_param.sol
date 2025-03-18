string constant fileLevelStr = "erc7201:test.file";
contract C {
    string constant stateVarStr = "erc7201:example.contract";
    function fileLevel() public pure returns (uint256) {
        return erc7201(fileLevelStr);
    }
    function stateVar() public pure returns (uint256) {
        return erc7201(stateVarStr);
    }
}
// ----
// fileLevel() -> -53040825928598508646084274422947713721470074456702576018505059398123701131264
// stateVar() -> -25829503200359024302446027944439076478880378268394343704927625121430584230912
