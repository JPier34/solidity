function erc7201Mock(string memory id) pure returns (uint256) {
    return uint256(
        keccak256(bytes.concat(bytes32(uint256(keccak256(bytes(id))) - 1))) &
        ~bytes32(uint256(0xff))
    );
}

contract C {
    function stringLiteral() public pure returns (uint) {
        return erc7201("example.main");
    }
    function emptyString() public pure returns (uint) {
        return erc7201("");
    }
    function testEquivalence() public pure returns (bool) {
        return
            erc7201("") == erc7201Mock("") &&
            erc7201("example.main") == erc7201Mock("example.main");
    }
}
// ----
// stringLiteral() -> 10958655983261152271848436692291137275443024275653522991983264966744321209600
// emptyString() -> 30348469548119976384149824193117947667795829812057172845188107037932402691072
// testEquivalence() -> true
