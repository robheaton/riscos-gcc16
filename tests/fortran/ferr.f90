! ferr.f90 -- runtime error paths: run as  ferr bounds | alloc | stop3 | errstop | divzero | none
! Each case should end the program with a Fortran runtime error message (and a non-zero return code) except 'none'.
program ferr
  implicit none
  character(len=16) :: what
  integer :: a(5), i, k
  integer, allocatable :: p(:)
  double precision :: z
  call get_command_argument(1, what)
  write(*, '(a,a)') 'ferr: ', trim(what)
  k = 6
  select case (trim(what))
  case ('bounds')
    a = 1
    do i = 1, k
      a(i) = i                     ! needs -fcheck=bounds: "Index '6' of dimension 1 of array 'a' above upper bound of 5"
    end do
    write(*, '(a)') 'ferr: not reached with -fcheck=bounds'
  case ('alloc')
    allocate(p(2))
    allocate(p(3))                 ! already allocated: runtime error
  case ('stop3')
    stop 3
  case ('errstop')
    error stop 'error stop was called'
  case ('divzero')
    z = 0.0d0
    write(*, '(a,es10.3)') 'ferr: 1/0 = ', 1.0d0 / z
  case default
    write(*, '(a)') 'ferr: nothing to do'
  end select
  write(*, '(a)') 'ferr: done'
end program ferr
