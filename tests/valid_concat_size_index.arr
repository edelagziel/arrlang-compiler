{
    arr a{2};
    arr b{3};
    arr joined{5};
    scl size;
    scl value;

    a = [1, 2];
    b = [3, 4, 5];
    joined = a # b;
    size = !joined;
    value = joined : 4;
}
