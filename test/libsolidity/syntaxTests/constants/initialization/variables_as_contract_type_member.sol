library L {
    uint constant internal lx = 1;
}

contract C {
    uint constant public x = ~L.lx;
    C constant c = C(address(1));
}
