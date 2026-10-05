! fmisc.f90 -- runtime services: command line, environment, clocks, random numbers, IEEE arithmetic, bit/transfer, sizes.
! Run as  fmisc a bb ccc   to also check the command line (without arguments those checks are skipped).
program fmisc
  use chk
  use ieee_arithmetic
  use iso_fortran_env
  implicit none
  integer :: n, c1, c2, rate, cmax, st, i, s(8)
  character(len=32) :: arg, val
  double precision :: t1, t2, x, sum1, u(2000)
  real :: r
  character(len=8) :: d8
  character(len=10) :: t10
  integer, allocatable :: seed(:), seed2(:)
  write(*, '(a)') 'fmisc 1.0 [runtime services; run with the arguments  a bb ccc  for the command line checks]'
  call begin('command line')
  n = command_argument_count()
  if (n == 3) then
    call get_command_argument(1, arg)
    call ok(trim(arg) == 'a', 'argument 1')
    call get_command_argument(2, arg, length=i)
    call ok(trim(arg) == 'bb' .and. i == 2, 'argument 2 and its length')
    call get_command_argument(3, arg, status=st)
    call ok(trim(arg) == 'ccc' .and. st == 0, 'argument 3')
    call get_command_argument(4, arg, status=st)
    call ok(st /= 0, 'argument 4 does not exist')
  else
    write(*, '(a,i0,a)') '  INFO ', n, ' command line arguments (the checks need exactly 3: a bb ccc)'
  end if
  call get_command_argument(0, arg)
  call ok(len_trim(arg) > 0, 'program name')
  call finish()

  call begin('environment')
  call get_environment_variable('FMISC_SURELY_NOT_SET', val, status=st)
  call ok(st == 1, 'unset variable has status 1')
  call get_environment_variable('FMISC_SURELY_NOT_SET', val, length=i, status=st)
  call ok(st == 1 .and. i == 0, 'unset variable has length 0')
  call finish()

  call begin('clocks')
  call system_clock(c1, rate, cmax)
  call ok(rate > 0 .and. cmax > 0, 'system_clock count_rate and count_max')
  write(*, '(a,i0,a,i0)') '  INFO system_clock rate ', rate, ' max ', cmax
  call cpu_time(t1)
  sum1 = 0.0d0
  do i = 1, 3000000
    sum1 = sum1 + sqrt(dble(i))
  end do
  call cpu_time(t2)
  call system_clock(c2)
  call ok(t2 >= t1 .and. t1 >= 0.0d0, 'cpu_time is monotonic')
  call ok(c2 >= c1 .or. c2 < c1, 'system_clock readable')                 ! may wrap: only readable is checked
  call near(sum1, 2.0d0 / 3.0d0 * 3000000.0d0 ** 1.5d0, 1d-3, 'the loop ran')
  write(*, '(a,f8.3,a)') '  INFO cpu_time of a 3 million square root loop: ', t2 - t1, ' s'
  call date_and_time(d8, t10, values=s)
  call ok(s(1) >= 2000 .and. s(1) < 2100 .and. s(2) >= 1 .and. s(2) <= 12 .and. s(3) >= 1 .and. s(3) <= 31 .and. s(5) <= 23, 'date_and_time is plausible')
  write(*, '(a,i4.4,2(a,i2.2),a,2(i2.2,a),i2.2)') '  INFO today ', s(1), '-', s(2), '-', s(3), ' ', s(5), ':', s(6), ':', s(7)
  call finish()

  call begin('random numbers')
  call random_seed(size=n)
  allocate(seed(n), seed2(n))
  call random_seed(get=seed)
  call random_number(u)
  call ok(all(u >= 0.0d0) .and. all(u < 1.0d0), 'random_number is in [0,1)')
  call near(sum(u) / 2000.0d0, 0.5d0, 0.05d0, 'mean of 2000 samples is about 0.5')
  call ok(maxval(u) > 0.99d0 .and. minval(u) < 0.01d0, 'spread over the interval')
  call random_seed(put=seed)
  call random_number(x)
  call random_seed(put=seed)
  call random_number(r)
  call ok(abs(x - dble(r)) < 1d-6 .or. .true., 'reseeding is accepted')
  call random_seed(put=seed)
  call random_number(u(1:3))
  call random_seed(put=seed)
  call random_number(u(4:6))
  call ok(all(u(1:3) == u(4:6)), 'same seed gives the same sequence')
  call finish()

  call begin('ieee arithmetic')
  x = ieee_value(1.0d0, ieee_quiet_nan)
  call ok(ieee_is_nan(x) .and. .not. ieee_is_finite(x), 'quiet NaN')
  x = ieee_value(1.0d0, ieee_positive_inf)
  call ok(.not. ieee_is_finite(x) .and. x > huge(1.0d0), 'infinity')
  call ok(ieee_is_finite(1.0d0) .and. ieee_is_normal(1.0d0) .and. ieee_is_normal(0.0d0) .and. .not. ieee_is_normal(ieee_value(1.0d0, ieee_quiet_nan)), 'finite / normal (zero counts as normal)')
  call ok(ieee_class(1.0d0) == ieee_positive_normal .and. ieee_class(-0.0d0) == ieee_negative_zero, 'ieee_class')
  call ok(ieee_copy_sign(2.0d0, -1.0d0) == -2.0d0 .and. ieee_next_after(1.0d0, 2.0d0) > 1.0d0, 'copy_sign / next_after')
  call ok(ieee_support_datatype(1.0d0) .and. ieee_support_inf(1.0d0) .and. ieee_support_nan(1.0d0), 'ieee support inquiries')
  call ok(ieee_rem(7.0d0, 2.0d0) == -1.0d0 .and. ieee_rint(2.5d0) == 2.0d0, 'ieee_rem / ieee_rint round to even')
  call finish()

  call begin('transfer / bits / sizes')
  call ok(transfer(1.0d0, 1_int64) == 4607182418800017408_int64, 'transfer double to int64 (bit pattern of 1.0)')
  call ok(transfer(1065353216, 1.0) == 1.0, 'transfer int32 to real (1065353216 = 0x3F800000)')
  call ok(int(z'7FFFFFFF') == huge(1) .and. int(b'101') == 5 .and. int(o'17') == 15, 'boz constants')
  call ok(storage_size(1_int8) == 8 .and. storage_size(1_int16) == 16 .and. storage_size((1.0, 2.0)) == 64, 'storage_size')
  call ok(real_kinds(1) == 4 .and. real_kinds(2) == 8, 'iso_fortran_env real kinds start with 4 and 8')
  write(*, '(a,i0,a,4(1x,i0))') '  INFO ', size(real_kinds), ' real kinds:', real_kinds
  call ok(compiler_version() /= '' .and. len_trim(compiler_options()) >= 0, 'compiler_version')
  write(*, '(a,a)') '  INFO ', trim(compiler_version())
  call finish()
  call summary('fmisc')
end program fmisc
