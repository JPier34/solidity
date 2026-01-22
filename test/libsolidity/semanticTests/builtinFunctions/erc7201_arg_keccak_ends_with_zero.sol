function erc7201Mock(string memory id) pure returns (uint256) {
    return uint256(
        keccak256(bytes.concat(bytes32(uint256(keccak256(bytes(id))) - 1))) &
        ~bytes32(uint256(0xff))
    );
}

contract C {
    function test() public pure returns (bool) {
        string memory s = "85";
        bytes32 h = keccak256(bytes(s));

        assert(uint8(h[31]) == 0);

        return erc7201(s) == erc7201Mock(s);
    }
}
// ----
// test() -> true
