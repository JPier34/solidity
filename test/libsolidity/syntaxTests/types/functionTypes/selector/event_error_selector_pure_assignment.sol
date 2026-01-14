==== Source: A.sol ====

event EvAExt();
error ErAExt();

contract A {
    event Ev();
    error Er();
}

==== Source: C.sol ====
import * as AMod from "A.sol";

event EvExt();
error ErExt();

bytes32 constant eventExtSelectorGlobal = EvExt.selector;
bytes4 constant errorExtSelectorGlobal = ErExt.selector;

bytes32 constant eventAExtSelectorGlobal = AMod.EvAExt.selector;
bytes4 constant errorAExtSelectorGlobal = AMod.ErAExt.selector;

bytes32 constant eventASelectorGlobal = AMod.A.Ev.selector;
bytes4 constant errorASelectorGlobal = AMod.A.Er.selector;

contract C {
    event Ev();
    error Er();

    bytes4 constant errorExtSelector = ErExt.selector;
    bytes32 constant eventExtSelector = EvExt.selector;

    bytes4 constant errorSelector = Er.selector;
    bytes32 constant eventSelector = Ev.selector;

    bytes4 constant errorSelectorC = C.Er.selector;
    bytes32 constant eventSelectorC = C.Ev.selector;

    bytes32 constant eventAExtSelector = AMod.EvAExt.selector;
    bytes4 constant errorAExtSelector = AMod.ErAExt.selector;

    bytes32 constant eventASelector = AMod.A.Ev.selector;
    bytes4 constant errorASelector = AMod.A.Er.selector;
}
// ----
