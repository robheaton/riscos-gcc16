! fcinterop.f90 -- Fortran calling C and C calling Fortran through iso_c_binding (link with cside.o).
module callbacks
  use iso_c_binding
  implicit none
  integer, save :: acc = 0
  integer(c_int), bind(c, name='c_counter') :: counter       ! a C global variable
contains
  subroutine add_to_acc(i) bind(c, name='f_add_to_acc')
    integer(c_int), value :: i
    acc = acc + i
  end subroutine
end module

program fcinterop
  use chk
  use iso_c_binding
  use callbacks
  implicit none
  interface
    function c_add(a, b) bind(c, name='c_add')
      import :: c_int
      integer(c_int), value :: a, b
      integer(c_int) :: c_add
    end function
    function c_dot(x, y, n) bind(c, name='c_dot')
      import :: c_double, c_int
      real(c_double), intent(in) :: x(*), y(*)
      integer(c_int), value :: n
      real(c_double) :: c_dot
    end function
    subroutine c_fill(a, n) bind(c, name='c_fill')
      import :: c_int
      integer(c_int), intent(out) :: a(*)
      integer(c_int), value :: n
    end subroutine
    function c_strlen(s) bind(c, name='c_strlen')
      import :: c_char, c_int
      character(kind=c_char), intent(in) :: s(*)
      integer(c_int) :: c_strlen
    end function
    subroutine c_upcase(s) bind(c, name='c_upcase')
      import :: c_char
      character(kind=c_char), intent(inout) :: s(*)
    end subroutine
    subroutine c_each(f, n) bind(c, name='c_each')
      import :: c_funptr, c_int
      type(c_funptr), value :: f
      integer(c_int), value :: n
    end subroutine
    function c_pair_sum(p) bind(c, name='c_pair_sum')
      import :: c_double, c_ptr
      type(c_ptr), value :: p
      real(c_double) :: c_pair_sum
    end function
  end interface
  type, bind(c) :: pair
    integer(c_int) :: a
    real(c_double) :: b
  end type
  real(c_double) :: x(3), y(3)
  integer(c_int) :: a(5)
  character(kind=c_char, len=8) :: s
  type(pair), target :: p
  integer :: i
  write(*, '(a)') 'fcinterop 1.0 [Fortran <-> C]'
  call begin('calls into C')
  call ok(c_add(2, 40) == 42, 'int arguments by value')
  x = [1.0_c_double, 2.0_c_double, 3.0_c_double]; y = [4.0_c_double, 5.0_c_double, 6.0_c_double]
  call ok(c_dot(x, y, 3) == 32.0_c_double, 'double arrays by reference')
  call c_fill(a, 5)
  call ok(all(a == [0, 1, 4, 9, 16]), 'intent(out) integer array')
  s = 'hello' // c_null_char
  call ok(c_strlen(s) == 5, 'C string length')
  call c_upcase(s)
  call ok(s(1:5) == 'HELLO', 'C modifies a Fortran character buffer')
  p = pair(3, 0.5_c_double)
  call ok(c_pair_sum(c_loc(p)) == 3.5_c_double, 'bind(c) derived type through c_loc')
  call ok(counter == 100, 'a C global variable')
  counter = counter + 1
  call ok(counter == 101, 'a C global variable can be written')
  call finish()
  call begin('calls back into Fortran')
  acc = 0
  call c_each(c_funloc(add_to_acc), 4)
  call ok(acc == 10, 'bind(c) callback called from C')
  call finish()
  call summary('fcinterop')
end program fcinterop
