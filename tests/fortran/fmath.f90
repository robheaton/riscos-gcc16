! fmath.f90 -- special functions (libm: tgamma lgamma erf erfc j0 j1 y0 y1 jn yn ...) and numerical kernels with known answers.
program fmath
  use chk
  use iso_fortran_env, only: real64
  implicit none
  integer :: i
  write(*, '(a)') 'fmath 1.0 [special functions and numerical kernels]'
  call test_special()
  call test_kernels()
  call test_complex_functions()
  call summary('fmath')

contains

  subroutine test_special()
    call begin('special functions')
    call near(gamma(5.0d0), 24.0d0, 1d-13, 'gamma(5)')
    call near(gamma(0.5d0), 1.7724538509055159d0, 1d-13, 'gamma(1/2)')
    call near(log_gamma(10.0d0), 12.801827480081469d0, 1d-13, 'log_gamma(10)')
    call near(erf(0.5d0), 0.5204998778130465d0, 1d-14, 'erf(0.5)')
    call near(erfc(0.5d0), 0.4795001221869535d0, 1d-14, 'erfc(0.5)')
    call near(erf(0.5d0) + erfc(0.5d0), 1.0d0, 1d-15, 'erf + erfc')
    call near(bessel_j0(1.0d0), 0.7651976865579666d0, 1d-14, 'bessel_j0(1)')
    call near(bessel_j1(1.0d0), 0.44005058574493355d0, 1d-14, 'bessel_j1(1)')
    call near(bessel_y0(1.0d0), 0.08825696421567697d0, 1d-13, 'bessel_y0(1)')
    call near(bessel_y1(1.0d0), -0.7812128213002887d0, 1d-13, 'bessel_y1(1)')
    call near(bessel_jn(3, 2.0d0), 0.12894324947440206d0, 1d-13, 'bessel_jn(3,2)')
    call near(bessel_yn(2, 3.0d0), -0.16040039348492374d0, 1d-13, 'bessel_yn(2,3)')
    call near(norm2([3.0d0, 4.0d0, 12.0d0]), 13.0d0, 1d-15, 'norm2')
    call near(hypot(5.0d0, 12.0d0), 13.0d0, 1d-15, 'hypot')
    call near(acosh(2.0d0), 1.3169578969248166d0, 1d-14, 'acosh(2)')
    call near(asinh(1.0d0), 0.881373587019543d0, 1d-14, 'asinh(1)')
    call near(atanh(0.5d0), 0.5493061443340548d0, 1d-14, 'atanh(0.5)')
    call near(real(gamma(5.0), real64), 24.0d0, 1d-5, 'single gamma(5)')
    call finish()
  end subroutine

  subroutine test_kernels()
    double precision :: h, s, y, t, x, pi_machin, lambda
    double precision :: a(3, 3), v(3), w(3), l(3, 3), u(3, 3), b(3), z(3)
    integer :: n, k, j
    complex(real64) :: f(8), g(8)
    call begin('numerical kernels')
    ! Simpson integration of sin(x) over [0, pi]
    n = 1000; h = 3.141592653589793d0 / n; s = sin(0.0d0) + sin(3.141592653589793d0)
    do i = 1, n - 1
      s = s + merge(2.0d0, 4.0d0, mod(i, 2) == 0) * sin(i * h)
    end do
    call near(s * h / 3.0d0, 2.0d0, 1d-10, 'Simpson integral of sin')
    ! Newton iteration for sqrt(2)
    x = 1.0d0
    do i = 1, 8
      x = 0.5d0 * (x + 2.0d0 / x)
    end do
    call near(x, 1.4142135623730951d0, 1d-15, 'Newton sqrt(2)')
    ! pi by Machin's formula
    pi_machin = 4.0d0 * (4.0d0 * atan(1.0d0 / 5.0d0) - atan(1.0d0 / 239.0d0))
    call near(pi_machin, 3.141592653589793d0, 1d-15, 'pi by Machin')
    ! RK4 for y' = -y, y(0) = 1, to t = 1
    y = 1.0d0; h = 0.01d0
    do i = 1, 100
      block
        double precision :: k1, k2, k3, k4
        k1 = -y; k2 = -(y + 0.5d0 * h * k1); k3 = -(y + 0.5d0 * h * k2); k4 = -(y + h * k3)
        y = y + h / 6.0d0 * (k1 + 2.0d0 * k2 + 2.0d0 * k3 + k4)
      end block
    end do
    call near(y, exp(-1.0d0), 1d-9, 'RK4 exp(-t)')
    ! harmonic series partial sum with Kahan compensation vs the exact asymptotic value
    s = 0.0d0; t = 0.0d0
    do i = 1, 100000
      block
        double precision :: term, tmp
        term = 1.0d0 / i - t
        tmp = s + term
        t = (tmp - s) - term
        s = tmp
      end block
    end do
    call near(s, 12.090146129863427d0, 1d-12, 'Kahan harmonic sum H(100000)')
    ! power method for the dominant eigenvalue of a symmetric matrix [[2,1,0],[1,2,1],[0,1,2]] = 2 + sqrt(2)
    a = reshape([2.0d0, 1.0d0, 0.0d0, 1.0d0, 2.0d0, 1.0d0, 0.0d0, 1.0d0, 2.0d0], [3, 3])
    v = [1.0d0, 1.0d0, 1.0d0]
    do i = 1, 200
      w = matmul(a, v)
      lambda = norm2(w)
      v = w / lambda
    end do
    call near(lambda, 2.0d0 + sqrt(2.0d0), 1d-12, 'power method eigenvalue')
    ! LU decomposition (Doolittle) and solve A x = b with A as above, b = [4, 8, 8] -> x = [1, 2, 3]
    l = 0.0d0; u = 0.0d0
    do j = 1, 3
      l(j, j) = 1.0d0
    end do
    do i = 1, 3
      do k = i, 3
        u(i, k) = a(i, k) - dot_product(l(i, 1:i - 1), u(1:i - 1, k))
      end do
      do k = i + 1, 3
        l(k, i) = (a(k, i) - dot_product(l(k, 1:i - 1), u(1:i - 1, i))) / u(i, i)
      end do
    end do
    b = [4.0d0, 8.0d0, 8.0d0]
    z = 0.0d0
    do i = 1, 3
      z(i) = b(i) - dot_product(l(i, 1:i - 1), z(1:i - 1))
    end do
    v = 0.0d0
    do i = 3, 1, -1
      v(i) = (z(i) - dot_product(u(i, i + 1:3), v(i + 1:3))) / u(i, i)
    end do
    call near(v(1), 1.0d0, 1d-13, 'LU solve x1')
    call near(v(2), 2.0d0, 1d-13, 'LU solve x2')
    call near(v(3), 3.0d0, 1d-13, 'LU solve x3')
    call near(maxval(abs(matmul(l, u) - a)), 0.0d0, 1d-14, 'L*U reproduces A')
    ! length-8 DFT of a ramp against the closed form X[k] = -4 + 4i cot(pi k / 8) for k > 0, X[0] = 28
    do i = 1, 8
      f(i) = cmplx(dble(i - 1), 0.0d0, real64)
    end do
    do k = 0, 7
      g(k + 1) = (0.0d0, 0.0d0)
      do j = 0, 7
        g(k + 1) = g(k + 1) + f(j + 1) * exp(cmplx(0.0d0, -2.0d0 * 3.141592653589793d0 * j * k / 8.0d0, real64))
      end do
    end do
    call near(real(g(1)), 28.0d0, 1d-12, 'DFT X[0]')
    do k = 1, 7
      call near(real(g(k + 1)), -4.0d0, 1d-12, 'DFT real part')
      call near(aimag(g(k + 1)), 4.0d0 / tan(3.141592653589793d0 * k / 8.0d0), 1d-12, 'DFT imaginary part')
    end do
    call finish()
  end subroutine

  subroutine test_complex_functions()
    complex(real64) :: z
    call begin('complex functions')
    z = (1.0d0, 1.0d0)
    call near(real(exp(z)), 1.4686939399158851d0, 1d-14, 'exp(1+i) real')
    call near(aimag(exp(z)), 2.2873552871788423d0, 1d-14, 'exp(1+i) imag')
    call near(real(log(z)), 0.34657359027997264d0, 1d-14, 'log(1+i) real')
    call near(aimag(log(z)), 0.7853981633974483d0, 1d-14, 'log(1+i) imag')
    call near(real(sin(z)), 1.2984575814159773d0, 1d-14, 'sin(1+i) real')
    call near(aimag(sin(z)), 0.6349639147847361d0, 1d-14, 'sin(1+i) imag')
    call near(real(cos(z)), 0.8337300251311491d0, 1d-14, 'cos(1+i) real')
    call near(aimag(cos(z)), -0.9888977057628651d0, 1d-14, 'cos(1+i) imag')
    call near(real(sqrt(z)), 1.0986841134678098d0, 1d-14, 'sqrt(1+i) real')
    call near(aimag(sqrt(z)), 0.45508986056222733d0, 1d-14, 'sqrt(1+i) imag')
    call near(abs(z**10), 32.0d0, 1d-12, '|(1+i)**10|')
    call finish()
  end subroutine
end program fmath
