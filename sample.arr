{
    scl x;
    scl size;
    scl value;
    arr a{4};
    arr b{4};
    arr c{4};
    arr joined{8};

    x = 5 + 3 * 2;
    a = [1, 2, 3, 4];
    b = [40, 10, 30, 20];

    c = a + b;
    c = c * 2;
    joined = a # b;
    size = !joined;
    value = a : 2;

    print "Scalar x": x;
    print "Array a": a;
    print "Array b": b;
    print "Array c": c;
    print "Joined and size": joined, size;
    print "Indexed value": value;

    if value {
        print "If branch": value;
    }
    else {
        print "Else branch": 0;
    }

    loop 2 {
        x = x + 1;
        print "Loop x": x;
    }

    ~a;
    $b;
    print "Reversed a": a;
    print "Sorted b": b;
}
