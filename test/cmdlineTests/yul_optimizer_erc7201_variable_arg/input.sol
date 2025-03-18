// SPDX-License-Identifier: GPL-3.0
pragma solidity >=0.0;
contract C {
    string id = "example.main";
    function f() public view returns (uint) {
        return erc7201(id);
    }
}
