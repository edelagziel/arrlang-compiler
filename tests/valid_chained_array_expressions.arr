{
    arr a{3};
    arr b{3};
    arr c{3};
    arr joined{6};

    a = [1, 2, 3];
    b = [4, 5, 6];
    c = (a + b) * 2;
    joined = (a + 1) # (b - 1);
}
