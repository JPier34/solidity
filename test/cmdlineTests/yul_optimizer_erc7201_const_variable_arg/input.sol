// SPDX-License-Identifier: GPL-3.0
pragma solidity >=0.0;
contract C {
    string constant const = "example.main";
    function f() public pure returns (uint) {
        return erc7201(const);
    }
}
