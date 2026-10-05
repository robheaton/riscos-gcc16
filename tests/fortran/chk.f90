! chk.f90 -- tiny self-checking harness shared by the Fortran regression programs (module chk).
module chk
  implicit none
  integer, save :: n_checks = 0, n_failed = 0, section_checks = 0
  character(len=24), save :: section_name = ' '
contains
  subroutine begin(name)
    character(len=*), intent(in) :: name
    section_name = name
    section_checks = 0
  end subroutine
  subroutine finish()
    write(*, '(2x,a,1x,i4,a)') trim(section_name), section_checks, ' checks'
  end subroutine
  subroutine ok(cond, what)
    logical, intent(in) :: cond
    character(len=*), intent(in) :: what
    n_checks = n_checks + 1
    section_checks = section_checks + 1
    if (.not. cond) then
      n_failed = n_failed + 1
      write(*, '(4x,a,a,a,a)') 'FAIL [', trim(section_name), '] ', what
    end if
  end subroutine
  subroutine near(x, y, tol, what)          ! relative tolerance (absolute near zero)
    double precision, intent(in) :: x, y, tol
    character(len=*), intent(in) :: what
    call ok(abs(x - y) <= tol * max(1.0d0, abs(y)), what)
  end subroutine
  subroutine nearf(x, y, tol, what)
    real, intent(in) :: x, y, tol
    character(len=*), intent(in) :: what
    call ok(abs(x - y) <= tol * max(1.0, abs(y)), what)
  end subroutine
  subroutine summary(prog)
    character(len=*), intent(in) :: prog
    write(*, '(a,a,a,i0,a,i0,a,a)') 'SUMMARY [', prog, ']: ', n_checks, ' checks, ', n_failed, ' failed -> ', &
         merge('FAIL', 'PASS', n_failed > 0)
    if (n_failed > 0) stop 1
  end subroutine
end module chk
