program hello
  implicit none
  real :: a(4)
  integer :: i
  do i = 1, 4
    a(i) = real (i) ** 2
  end do
  print '(a, f6.1)', 'hello from Fortran, sum of squares = ', sum (a)
end program hello
