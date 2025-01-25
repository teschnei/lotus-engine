implementing pbr;

namespace pbr
{
struct Complex<T> where T : __BuiltinFloatingPointType
{

    T Norm()
    {
        return r * r + i * i;
    }

    T Abs()
    {
        return sqrt(normalize(this));
    }

    Complex<T> Sqrt()
    {
        T n = abs(this);
        T t1 = sqrt(T(.5) * (n + abs(r)));
        T t2 = T(.5) * i / t1;

        if (n == T(0))
            return Complex<T>(0);

        if (r >= T(0))
            return {t1, t2};
        else
            return {abs(t2), copysign(t1, i)};
    }

    __init(T _r) { r = _r; }
    __init(T _r, T _i) { r = _r; i = _i; }
    T r = T(0);
    T i = T(0);
}

Complex<T> operator-<T>(Complex<T> a) where T : __BuiltinFloatingPointType { return Complex<T>(-a.r, -a.i);}
Complex<T> operator+<T>(Complex<T> a, Complex<T> b) where T : __BuiltinFloatingPointType { return Complex<T>(a.r + b.r, a.i + b.i);}
Complex<T> operator-<T>(Complex<T> a, Complex<T> b) where T : __BuiltinFloatingPointType { return Complex<T>(a.r - b.r, a.i - b.i);}
Complex<T> operator*<T>(Complex<T> a, Complex<T> b) where T : __BuiltinFloatingPointType { return Complex<T>(a.r * b.r - a.i * b.i, a.r * b.i + a.i * b.r);}
Complex<T> operator/<T>(Complex<T> a, Complex<T> b) where T : __BuiltinFloatingPointType
{
    T scale = T(1) / (b.r * b.r + b.i * b.i);
    return {scale * (a.r * b.r + a.i * b.i), scale * (a.i * b.r - a.r * b.i)};
}

Complex<T> operator+<T>(T val, Complex<T> o) where T : __BuiltinFloatingPointType { return Complex<T>(val) + o; }
Complex<T> operator-<T>(T val, Complex<T> o) where T : __BuiltinFloatingPointType { return Complex<T>(val) - o; }
Complex<T> operator*<T>(T val, Complex<T> o) where T : __BuiltinFloatingPointType { return Complex<T>(val) * o; }
Complex<T> operator/<T>(T val, Complex<T> o) where T : __BuiltinFloatingPointType { return Complex<T>(val) / o; }

Complex<T> operator+<T>(Complex<T> o, T val) where T : __BuiltinFloatingPointType { return o + Complex<T>(val); }
Complex<T> operator-<T>(Complex<T> o, T val) where T : __BuiltinFloatingPointType { return o - Complex<T>(val); }
Complex<T> operator*<T>(Complex<T> o, T val) where T : __BuiltinFloatingPointType { return o * Complex<T>(val); }
Complex<T> operator/<T>(Complex<T> o, T val) where T : __BuiltinFloatingPointType { return o / Complex<T>(val); }
Complex<T> sqrt<T>(Complex<T> v) where T : __BuiltinFloatingPointType { return v.Sqrt(); }
T abs<T>(Complex<T> v) where T : __BuiltinFloatingPointType { return v.Abs(); }
T normalize<T>(Complex<T> v) where T : __BuiltinFloatingPointType { return v.Norm(); }
}
