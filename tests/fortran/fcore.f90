! fcore.f90 -- core language: integers, reals, complex, arrays, derived types, OOP, strings, internal I/O, intrinsics.
module shapes_m
  implicit none
  type, abstract :: shape_t
  contains
    procedure(area_i), deferred :: area
    procedure :: describe
  end type
  abstract interface
    function area_i(self) result(a)
      import :: shape_t
      class(shape_t), intent(in) :: self
      double precision :: a
    end function
  end interface
  type, extends(shape_t) :: rect_t
    double precision :: w = 0, h = 0
  contains
    procedure :: area => rect_area
  end type
  type, extends(shape_t) :: circle_t
    double precision :: r = 0
  contains
    procedure :: area => circle_area
  end type
  type :: vec2
    double precision :: x = 0, y = 0
  end type
  interface operator(+)
    module procedure vadd
  end interface
  interface operator(*)
    module procedure vscale
  end interface
contains
  function rect_area(self) result(a)
    class(rect_t), intent(in) :: self
    double precision :: a
    a = self%w * self%h
  end function
  function circle_area(self) result(a)
    class(circle_t), intent(in) :: self
    double precision :: a
    a = 3.141592653589793d0 * self%r**2
  end function
  function describe(self) result(s)
    class(shape_t), intent(in) :: self
    character(len=16) :: s
    select type (self)
    type is (rect_t);   s = 'rect'
    type is (circle_t); s = 'circle'
    class default;      s = 'unknown'
    end select
  end function
  pure function vadd(a, b) result(c)
    type(vec2), intent(in) :: a, b
    type(vec2) :: c
    c = vec2(a%x + b%x, a%y + b%y)
  end function
  function triple(x) result(y)
    real, intent(in) :: x
    real :: y
    y = 3.0 * x
  end function
  pure function vscale(k, a) result(c)
    double precision, intent(in) :: k
    type(vec2), intent(in) :: a
    type(vec2) :: c
    c = vec2(k * a%x, k * a%y)
  end function
end module shapes_m

program fcore
  use chk
  use shapes_m
  use iso_fortran_env, only: int8, int16, int32, int64, real32, real64
  implicit none
  integer :: i, j, k
  double precision :: d
  write(*, '(a)') 'fcore 1.0 [' // 'gfortran ' // 'core language]'
  call test_integers()
  call test_reals()
  call test_complex()
  call test_arrays()
  call test_types()
  call test_strings()
  call test_internal_io()
  call test_control()
  call test_inquiry()
  call summary('fcore')

contains

  subroutine test_integers()
    integer(int8) :: a8
    integer(int64) :: big
    call begin('integers')
    call ok(7 / 2 == 3 .and. (-7) / 2 == -3, 'integer division truncates toward zero')
    call ok(mod(-7, 3) == -1 .and. modulo(-7, 3) == 2 .and. mod(7, -3) == 1 .and. modulo(7, -3) == -2, 'mod / modulo signs')
    call ok(2**10 == 1024 .and. (-2)**3 == -8 .and. 3**0 == 1, 'integer powers')
    big = 2_int64**40
    call ok(big == 1099511627776_int64 .and. big / 1024 == 1073741824_int64, 'integer(8) arithmetic')
    call ok(huge(1_int32) == 2147483647 .and. huge(1_int64) == 9223372036854775807_int64, 'huge')
    a8 = 127_int8
    call ok(a8 == 127 .and. huge(a8) == 127 .and. bit_size(a8) == 8, 'integer(1) kind')
    call ok(iand(12, 10) == 8 .and. ior(12, 10) == 14 .and. ieor(12, 10) == 6 .and. not(0) == -1, 'bit operations')
    call ok(ishft(1, 4) == 16 .and. ishft(256, -4) == 16 .and. ishftc(1, 1, 8) == 2, 'shifts')
    call ok(popcnt(255) == 8 .and. leadz(1) == 31 .and. trailz(8) == 3, 'popcnt / leadz / trailz')
    call ok(btest(5, 0) .and. .not. btest(5, 1) .and. ibset(0, 3) == 8 .and. ibclr(15, 0) == 14, 'bit tests')
    call ok(abs(-5) == 5 .and. sign(3, -1) == -3 .and. max(1, 9, 4) == 9 .and. min(3, -2) == -2 .and. dim(5, 3) == 2, 'abs sign max min dim')
    call ok(gcd(48, 18) == 6 .and. gcd(17, 5) == 1, 'recursive gcd')
    call finish()
  end subroutine

  recursive function gcd(a, b) result(g)
    integer, intent(in) :: a, b
    integer :: g
    if (b == 0) then
      g = a
    else
      g = gcd(b, mod(a, b))
    end if
  end function

  subroutine test_reals()
    real(real32) :: s
    real(real64) :: x
    call begin('reals')
    x = 1.0d0
    call near(sin(x), 0.8414709848078965d0, 1d-14, 'sin(1)')
    call near(cos(x), 0.5403023058681398d0, 1d-14, 'cos(1)')
    call near(tan(x), 1.5574077246549023d0, 1d-14, 'tan(1)')
    call near(asin(0.5d0), 0.5235987755982989d0, 1d-14, 'asin(0.5)')
    call near(acos(0.5d0), 1.0471975511965979d0, 1d-14, 'acos(0.5)')
    call near(atan(x), 0.7853981633974483d0, 1d-14, 'atan(1)')
    call near(atan2(1.0d0, -1.0d0), 2.356194490192345d0, 1d-14, 'atan2(1,-1)')
    call near(sinh(x), 1.1752011936438014d0, 1d-14, 'sinh(1)')
    call near(cosh(x), 1.5430806348152437d0, 1d-14, 'cosh(1)')
    call near(tanh(x), 0.7615941559557649d0, 1d-14, 'tanh(1)')
    call near(exp(x), 2.718281828459045d0, 1d-14, 'exp(1)')
    call near(log(10.0d0), 2.302585092994046d0, 1d-14, 'log(10)')
    call near(log10(1000.0d0), 3.0d0, 1d-14, 'log10(1000)')
    call near(sqrt(2.0d0), 1.4142135623730951d0, 1d-15, 'sqrt(2)')
    call near(2.0d0**0.5d0, 1.4142135623730951d0, 1d-14, 'x**y')
    call near(hypot_(3.0d0, 4.0d0), 5.0d0, 1d-15, 'hypot')
    s = 2.0
    call nearf(sqrt(s), 1.4142135_real32, 1e-6, 'single sqrt(2)')
    call nearf(sin(1.0), 0.84147096, 2e-6, 'single sin(1)')
    call nearf(exp(1.0), 2.7182817, 2e-6, 'single exp(1)')
    call ok(nint(2.5d0) == 3 .and. nint(-2.5d0) == -3 .and. nint(2.4d0) == 2, 'nint rounds half away from zero')
    call ok(floor(-2.5d0) == -3 .and. ceiling(-2.5d0) == -2 .and. int(-2.9d0) == -2 .and. aint(2.7d0) == 2.0d0, 'floor / ceiling / int / aint')
    call ok(anint(2.5d0) == 3.0d0 .and. mod(5.5d0, 2.0d0) == 1.5d0 .and. modulo(-5.5d0, 2.0d0) == 0.5d0, 'anint / mod / modulo on reals')
    call ok(sign(2.0d0, -0.0d0) == -2.0d0 .and. abs(-3.5d0) == 3.5d0, 'sign with negative zero')
    call ok(fraction(8.0d0) == 0.5d0 .and. exponent(8.0d0) == 4 .and. scale(1.0d0, 3) == 8.0d0 .and. set_exponent(1.0d0, 3) == 4.0d0, 'floating point model')
    call ok(nearest(1.0d0, 1.0d0) > 1.0d0 .and. spacing(1.0d0) == epsilon(1.0d0) .and. rrspacing(1.0d0) > 0, 'nearest / spacing')
    call ok(0.1d0 + 0.2d0 /= 0.3d0 .and. abs(0.1d0 + 0.2d0 - 0.3d0) < 1d-15, 'binary floating point')
    d = 1.0d0 / 3.0d0
    call ok(d * 3.0d0 == 1.0d0 .and. real(d, real32) == 0.33333334_real32, 'precision conversion double -> single')
    call finish()
  end subroutine

  pure function hypot_(a, b) result(h)
    double precision, intent(in) :: a, b
    double precision :: h
    h = sqrt(a * a + b * b)
  end function

  subroutine test_complex()
    complex(real64) :: z, w
    complex :: c
    call begin('complex')
    z = (3.0d0, 4.0d0)
    w = (1.0d0, -2.0d0)
    call near(abs(z), 5.0d0, 1d-15, 'abs(3+4i)')
    call ok(z + w == (4.0d0, 2.0d0) .and. z - w == (2.0d0, 6.0d0), 'complex + -')
    call ok(z * w == (11.0d0, -2.0d0), 'complex *')
    call near(real(z / w), -1.0d0, 1d-14, 'complex / (real part)')
    call near(aimag(z / w), 2.0d0, 1d-14, 'complex / (imag part)')
    call ok(conjg(z) == (3.0d0, -4.0d0), 'conjg')
    call near(real(exp((0.0d0, 3.141592653589793d0))), -1.0d0, 1d-14, 'exp(i pi)')
    call near(aimag(sqrt((-4.0d0, 0.0d0))), 2.0d0, 1d-14, 'sqrt(-4) principal root')
    call near(real(z**2), -7.0d0, 1d-13, 'z**2 real')
    call near(aimag(z**2), 24.0d0, 1d-13, 'z**2 imag')
    c = cmplx(1.0, 2.0)
    call ok(c == (1.0, 2.0) .and. real(c * conjg(c)) == 5.0, 'single complex')
    call near(atan2(aimag(z), real(z)), 0.9272952180016122d0, 1d-14, 'argument')
    call finish()
  end subroutine

  subroutine test_arrays()
    integer :: a(5), b(2, 3), c(3, 2), m(3, 3)
    integer, allocatable :: v(:), w(:, :)
    double precision :: x(4), mat(2, 2), vec(2)
    logical :: mask(5)
    call begin('arrays')
    a = [(i * i, i = 1, 5)]
    call ok(sum(a) == 55 .and. product(a) == 14400 .and. maxval(a) == 25 .and. minval(a) == 1, 'sum product maxval minval')
    call ok(maxloc(a, 1) == 5 .and. minloc(a, 1) == 1 .and. count(a > 5) == 3 .and. any(a == 9) .and. all(a > 0), 'maxloc minloc count any all')
    call ok(all(a(5:1:-1) == [25, 16, 9, 4, 1]) .and. all(a(::2) == [1, 9, 25]), 'sections with strides')
    b = reshape([1, 2, 3, 4, 5, 6], [2, 3])
    call ok(b(1, 1) == 1 .and. b(2, 1) == 2 .and. b(1, 3) == 5 .and. b(2, 3) == 6, 'reshape is column major')
    c = transpose(b)
    call ok(c(3, 1) == 5 .and. c(1, 2) == 2 .and. all(shape(c) == [3, 2]), 'transpose')
    m = matmul(c, b)
    call ok(m(1, 1) == 5 .and. m(3, 3) == 61 .and. sum(m) == 225, 'matmul')
    call ok(dot_product([1, 2, 3], [4, 5, 6]) == 32, 'dot_product')
    mask = a > 4
    call ok(all(pack(a, mask) == [9, 16, 25]), 'pack')
    v = unpack([7, 8, 9], mask, 0)
    call ok(all(v == [0, 0, 7, 8, 9]), 'unpack')
    call ok(all(spread([1, 2], 1, 3) == reshape([1, 1, 1, 2, 2, 2], [3, 2])), 'spread')
    call ok(all(cshift([1, 2, 3, 4], 1) == [2, 3, 4, 1]) .and. all(eoshift([1, 2, 3, 4], 1) == [2, 3, 4, 0]), 'cshift eoshift')
    where (a > 10)
      a = -a
    elsewhere
      a = a + 100
    end where
    call ok(all(a == [101, 104, 109, -16, -25]), 'where / elsewhere')
    forall (i = 1:3, j = 1:3) m(i, j) = i * 10 + j
    call ok(m(2, 3) == 23 .and. m(3, 1) == 31, 'forall')
    allocate(w(0:2, 3))
    w = 0
    do j = 1, 3
      do i = 0, 2
        w(i, j) = i + 10 * j
      end do
    end do
    call ok(lbound(w, 1) == 0 .and. ubound(w, 1) == 2 .and. size(w) == 9 .and. w(2, 3) == 32, 'allocatable with bounds')
    deallocate(w)
    call ok(.not. allocated(w), 'deallocate')
    v = [3, 1, 2]                                  ! (re)allocation on assignment
    call sort_ints(v)
    call ok(all(v == [1, 2, 3]), 'sort via assumed-shape dummy')
    deallocate(v)
    mat = reshape([2.0d0, 1.0d0, 1.0d0, 3.0d0], [2, 2])
    vec = [5.0d0, 10.0d0]
    call solve2(mat, vec)                          ! 2x + y = 5, x + 3y = 10 -> x = 1, y = 3
    call near(vec(1), 1.0d0, 1d-14, 'linear solve x')
    call near(vec(2), 3.0d0, 1d-14, 'linear solve y')
    x = [(0.25d0 * i, i = 1, 4)]
    call near(sum(x * x), 0.0625d0 + 0.25d0 + 0.5625d0 + 1.0d0, 1d-15, 'elemental array expression')
    call ok(size(a) == 5 .and. rank(m) == 2 .and. all(shape(m) == [3, 3]), 'size rank shape')
    call finish()
  end subroutine

  subroutine sort_ints(v)
    integer, intent(inout) :: v(:)
    integer :: t, p, q
    do p = 1, size(v) - 1
      do q = 1, size(v) - p
        if (v(q) > v(q + 1)) then
          t = v(q); v(q) = v(q + 1); v(q + 1) = t
        end if
      end do
    end do
  end subroutine

  subroutine solve2(a, b)                          ! Gaussian elimination with partial pivoting, 2x2
    double precision, intent(inout) :: a(2, 2), b(2)
    double precision :: f
    integer :: p
    p = 1
    if (abs(a(2, 1)) > abs(a(1, 1))) p = 2
    if (p == 2) then
      a(1:2, :) = a(2:1:-1, :); b(1:2) = b(2:1:-1)
    end if
    f = a(2, 1) / a(1, 1)
    a(2, :) = a(2, :) - f * a(1, :)
    b(2) = b(2) - f * b(1)
    b(2) = b(2) / a(2, 2)
    b(1) = (b(1) - a(1, 2) * b(2)) / a(1, 1)
  end subroutine

  subroutine test_types()
    type(rect_t) :: r1
    type(circle_t) :: c1
    class(shape_t), allocatable :: s
    type(vec2) :: p, q
    class(*), allocatable :: any
    procedure(real), pointer :: fp
    call begin('types')
    r1 = rect_t(w=3.0d0, h=4.0d0)
    c1 = circle_t(r=2.0d0)
    call near(r1%area(), 12.0d0, 1d-15, 'type-bound area (rect)')
    call near(c1%area(), 12.566370614359172d0, 1d-14, 'type-bound area (circle)')
    allocate(s, source=c1)
    call ok(s%describe() == 'circle', 'polymorphic describe')
    call near(s%area(), 12.566370614359172d0, 1d-14, 'polymorphic dispatch')
    deallocate(s)
    p = vec2(1.0d0, 2.0d0)
    q = 2.0d0 * (p + vec2(1.0d0, 1.0d0))
    call ok(q%x == 4.0d0 .and. q%y == 6.0d0, 'operator overloading')
    allocate(any, source=42)
    select type (any)
    type is (integer);  call ok(any == 42, 'unlimited polymorphic integer')
    class default;      call ok(.false., 'unlimited polymorphic type')
    end select
    deallocate(any)
    fp => triple
    call nearf(fp(2.0), 6.0, 1e-6, 'procedure pointer')
    call ok(opt(1) == 1 .and. opt(1, 5) == 6 .and. opt(b=2, a=1) == 3, 'optional and keyword arguments')
    call ok(total([1, 2, 3, 4]) == 10 .and. total() == 0, 'array argument / optional')
    call ok(fib(20) == 6765, 'recursion')
    call finish()
  end subroutine

  function opt(a, b) result(c)
    integer, intent(in) :: a
    integer, intent(in), optional :: b
    integer :: c
    c = a
    if (present(b)) c = c + b
  end function
  function total(v) result(t)
    integer, intent(in), optional :: v(:)
    integer :: t
    t = 0
    if (present(v)) t = sum(v)
  end function
  recursive function fib(n) result(f)
    integer, intent(in) :: n
    integer :: f
    if (n < 2) then
      f = n
    else
      f = fib(n - 1) + fib(n - 2)
    end if
  end function

  subroutine test_strings()
    character(len=20) :: s, t
    character(len=:), allocatable :: u
    call begin('strings')
    s = 'Hello'
    call ok(len(s) == 20 .and. len_trim(s) == 5 .and. trim(s) // '!' == 'Hello!', 'len / len_trim / trim / //')
    call ok(index('abcabc', 'ca') == 3 .and. index('abcabc', 'ca', back=.true.) == 3 .and. index('abc', 'z') == 0, 'index')
    call ok(scan('hello world', 'ow') == 5 .and. verify('aabbcc', 'ab') == 5, 'scan verify')
    call ok(adjustl('   x') == 'x   ' .and. adjustr('x   ') == '   x', 'adjustl adjustr')
    call ok(repeat('ab', 3) == 'ababab' .and. s(2:4) == 'ell', 'repeat / substring')
    call ok(iachar('A') == 65 .and. achar(97) == 'a' .and. ichar('0') == 48 .and. char(66) == 'B', 'character codes')
    call ok(lge('b', 'a') .and. llt('a', 'b') .and. 'abc' < 'abd' .and. 'a' == 'a   ', 'comparisons ignore trailing blanks')
    t = 'x'
    t(3:5) = 'abc'
    call ok(t(1:5) == 'x ' // 'abc', 'substring assignment')
    u = 'dynamic'
    u = u // ' length'
    call ok(len(u) == 14 .and. u == 'dynamic length', 'deferred-length allocatable character')
    call ok(to_upper('MiXed 123') == 'MIXED 123', 'user function on strings')
    call ok(trim(transfer([104, 105], 'ab')) /= '', 'transfer integer to character does not crash')
    call finish()
  end subroutine

  function to_upper(s) result(r)
    character(len=*), intent(in) :: s
    character(len=len(s)) :: r
    integer :: n
    do n = 1, len(s)
      if (s(n:n) >= 'a' .and. s(n:n) <= 'z') then
        r(n:n) = achar(iachar(s(n:n)) - 32)
      else
        r(n:n) = s(n:n)
      end if
    end do
  end function

  subroutine test_internal_io()
    character(len=40) :: line
    integer :: n1, n2, ios
    double precision :: x, y
    character(len=8) :: word
    call begin('internal i/o')
    write(line, '(i5,1x,f8.3,1x,es12.4)') 42, 3.14159d0, 12345.678d0
    call ok(line(1:5) == '   42' .and. line(7:14) == '   3.142' .and. line(16:27) == '  1.2346E+04', 'formatted write to a string')
    read(line, *) n1, x, y
    call ok(n1 == 42, 'list-directed read: integer')
    call near(x, 3.142d0, 1d-12, 'list-directed read: real')
    call near(y, 12346.0d0, 1d-3, 'list-directed read: exponent form')
    write(line, '(a,i0,a,l1)') 'n=', 123, ' flag=', .true.
    call ok(trim(line) == 'n=123 flag=T', 'i0 and l1 edit descriptors')
    write(line, '(2(i3,","),i3)') 1, 22, 333
    call ok(trim(line) == '  1, 22,333', 'repeat counts')
    line = '7 8'
    read(line, *) n1, n2
    call ok(n1 == 7 .and. n2 == 8, 'read from a literal')
    line = 'abc'
    read(line, '(a)', iostat=ios) word
    call ok(ios == 0 .and. trim(word) == 'abc', 'read a')
    line = 'x'
    read(line, *, iostat=ios) n1
    call ok(ios /= 0, 'iostat reports a conversion error')
    write(line, '(f10.0)') 2.5d0
    call ok(adjustl(line(1:10)) == '3.' .or. adjustl(line(1:10)) == '2.', 'rounding of f10.0 (round to nearest even or away)')
    write(line, '(e15.7)') 0.000123456789d0
    call ok(adjustl(line(1:15)) == '0.1234568E-03', 'e format')
    write(line, '(g12.5)') 100000.0d0
    call ok(adjustl(line(1:12)) == '0.10000E+06', 'g format switches to exponent')
    write(line, '(i0)') -2147483647 - 1
    call ok(trim(line) == '-2147483648', 'most negative integer')
    write(line, '(a)') 'abc'
    call ok(line(1:3) == 'abc' .and. line(4:4) == ' ', 'a edit descriptor pads with blanks')
    call finish()
  end subroutine

  subroutine test_control()
    integer :: s, k2, cnt
    character(len=3) :: name
    call begin('control flow')
    s = 0
    do i = 1, 10
      if (mod(i, 2) == 0) cycle
      if (i > 7) exit
      s = s + i
    end do
    call ok(s == 16, 'do / cycle / exit')
    k2 = 0
    outer: do i = 1, 5
      do j = 1, 5
        if (i * j > 6) exit outer
        k2 = k2 + 1
      end do
    end do outer
    call ok(k2 == 8, 'named exit')
    cnt = 0
    do while (cnt < 7)
      cnt = cnt + 3
    end do
    call ok(cnt == 9, 'do while')
    do concurrent (i = 1:4)
      k2 = i
    end do
    call ok(k2 >= 1 .and. k2 <= 4, 'do concurrent')
    do k = 3, 1, -1
      s = s * 2
    end do
    call ok(s == 128, 'negative step')
    do i = 1, 3
      select case (i)
      case (1);    name = 'one'
      case (2, 3); name = 'few'
      case default; name = '???'
      end select
    end do
    call ok(name == 'few', 'select case')
    call ok(merge(1, 2, .true.) == 1 .and. merge(1, 2, .false.) == 2, 'merge')
    if (.false.) goto 100
    k = 1
100 continue
    call ok(k == 1, 'goto / label')
    call finish()
  end subroutine

  subroutine test_inquiry()
    call begin('inquiry')
    call ok(huge(1.0) > 3.0e38 .and. tiny(1.0) < 1.2e-38 .and. epsilon(1.0) == 1.1920929e-7, 'single model numbers')
    call ok(huge(1.0d0) > 1.7d308 .and. tiny(1.0d0) < 2.3d-308 .and. epsilon(1.0d0) == 2.220446049250313d-16, 'double model numbers')
    call ok(precision(1.0) == 6 .and. precision(1.0d0) == 15 .and. range(1.0) == 37 .and. range(1.0d0) == 307, 'precision / range')
    call ok(selected_real_kind(6) == 4 .and. selected_real_kind(15) == 8 .and. selected_int_kind(9) == 4 .and. selected_int_kind(18) == 8, 'selected kinds')
    call ok(kind(1.0) == 4 .and. kind(1.0d0) == 8 .and. kind(1) == 4 .and. kind(.true.) == 4, 'default kinds')
    call ok(storage_size(1.0d0) == 64 .and. storage_size(1) == 32 .and. digits(1.0d0) == 53 .and. radix(1.0) == 2, 'storage_size digits radix')
    call ok(isnan_(-1.0d0) .eqv. .false. .and. .not. isnan_(sqrt(2.0d0)), 'nan test')
    call ok(maxexponent(1.0d0) == 1024 .and. minexponent(1.0d0) == -1021, 'exponent range')
    call finish()
  end subroutine

  pure function isnan_(x) result(b)
    double precision, intent(in) :: x
    logical :: b
    b = x /= x
  end function

end program fcore
