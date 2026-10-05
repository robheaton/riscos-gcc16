! fio.f90 -- file I/O: formatted/unformatted sequential, stream, direct access, inquire, iostat/iomsg, namelist, scratch files.
program fio
  use chk
  implicit none
  integer :: u, ios, i, n, v(5), w(5)
  character(len=80) :: line, msg
  character(len=*), parameter :: f1 = 'fio_t1', f2 = 'fio_t2', f3 = 'fio_t3', f4 = 'fio_t4'
  logical :: ex, op
  double precision :: x, y
  write(*, '(a)') 'fio 1.0 [file i/o; creates and deletes fio_t1..fio_t4 in the current directory]'
  call test_formatted()
  call test_unformatted()
  call test_stream_direct()
  call test_inquire_errors()
  call test_namelist_scratch()
  call summary('fio')

contains

  subroutine test_formatted()
    call begin('formatted sequential')
    open(newunit=u, file=f1, status='replace', action='write', form='formatted', iostat=ios)
    call ok(ios == 0, 'open for writing')
    do i = 1, 5
      write(u, '(i3,2x,f6.2,2x,a)') i, i * 1.5d0, 'line'
    end do
    write(u, '(a)') 'last line without numbers'
    close(u)
    open(newunit=u, file=f1, status='old', action='read', iostat=ios)
    call ok(ios == 0, 'reopen for reading')
    n = 0
    do
      read(u, '(a)', iostat=ios) line
      if (ios /= 0) exit
      n = n + 1
      if (n == 3) call ok(line(1:3) == '  3' .and. line(6:11) == '  4.50', 'record 3 contents')
    end do
    call ok(n == 6 .and. is_iostat_end(ios), 'six records then end of file')
    rewind(u)
    read(u, *) i, x
    call ok(i == 1 .and. abs(x - 1.5d0) < 1d-12, 'rewind and list-directed read')
    backspace(u)
    read(u, '(a)') line
    call ok(line(1:3) == '  1', 'backspace')
    close(u)
    open(newunit=u, file=f1, status='old', position='append', action='write')
    write(u, '(a)') 'appended'
    close(u)
    open(newunit=u, file=f1, status='old', action='read')
    n = 0
    do
      read(u, '(a)', iostat=ios) line
      if (ios /= 0) exit
      n = n + 1
    end do
    call ok(n == 7 .and. trim(line) == 'appended', 'append adds a record')
    close(u, status='delete')
    inquire(file=f1, exist=ex)
    call ok(.not. ex, 'close with status=delete removes the file')
    call finish()
  end subroutine

  subroutine test_unformatted()
    double precision :: d(4), e(4)
    call begin('unformatted sequential')
    d = [1.5d0, -2.25d0, 3.0d100, 4.0d-100]
    open(newunit=u, file=f2, status='replace', form='unformatted', action='write')
    write(u) 42, 'abc'
    write(u) d
    write(u) [1, 2, 3]
    close(u)
    open(newunit=u, file=f2, status='old', form='unformatted', action='read')
    read(u) i, line(1:3)
    call ok(i == 42 .and. line(1:3) == 'abc', 'record 1')
    read(u) e
    call ok(all(e == d), 'record 2: doubles are bit-exact')
    v = 0
    read(u) v(1:3)
    call ok(all(v(1:3) == [1, 2, 3]), 'record 3')
    read(u, iostat=ios) i
    call ok(is_iostat_end(ios), 'end of file after the last record')
    rewind(u)
    read(u) i                                        ! reading less than a record skips the rest
    read(u) e
    call ok(all(e == d), 'partial read skips the remainder of the record')
    close(u, status='delete')
    call finish()
  end subroutine

  subroutine test_stream_direct()
    integer(1) :: bytes(4)
    integer :: k, rec(2)
    call begin('stream and direct access')
    open(newunit=u, file=f3, status='replace', access='stream', form='unformatted')
    write(u) 1, 2, 3, 4                              ! 16 bytes
    write(u, pos=5) 99                               ! overwrite the second integer
    close(u)
    open(newunit=u, file=f3, status='old', access='stream', form='unformatted', action='read')
    inquire(unit=u, size=n)
    call ok(n == 16, 'stream file size is 16 bytes')
    read(u, pos=1) v(1:4)
    call ok(all(v(1:4) == [1, 99, 3, 4]), 'stream write with pos=')
    read(u, pos=9) bytes
    call ok(bytes(1) == 3 .and. bytes(2) == 0, 'little endian bytes')
    close(u, status='delete')
    open(newunit=u, file=f4, status='replace', access='direct', recl=8, form='unformatted')
    do k = 10, 1, -1
      write(u, rec=k) k, k * k
    end do
    read(u, rec=7) rec
    call ok(rec(1) == 7 .and. rec(2) == 49, 'direct access read of record 7')
    read(u, rec=1) rec
    call ok(rec(1) == 1 .and. rec(2) == 1, 'direct access read of record 1')
    close(u, status='delete')
    call finish()
  end subroutine

  subroutine test_inquire_errors()
    call begin('inquire and errors')
    inquire(file='fio_does_not_exist', exist=ex)
    call ok(.not. ex, 'inquire exist=.false.')
    open(unit=17, file='fio_does_not_exist', status='old', iostat=ios, iomsg=msg)
    call ok(ios /= 0 .and. len_trim(msg) > 0, 'open of a missing file fails with an iomsg')
    open(unit=18, file=f1, status='replace')
    inquire(unit=18, opened=op, number=n)
    call ok(op .and. n == 18, 'inquire unit opened / number')
    inquire(file=f1, exist=ex, opened=op)
    call ok(ex .and. op, 'inquire by file')
    write(18, '(a)') 'x'
    close(18, status='delete')
    call ok(.true., 'close')
    call finish()
  end subroutine

  subroutine test_namelist_scratch()
    integer :: a, b
    double precision :: c
    character(len=8) :: nm
    namelist /params/ a, b, c, nm
    call begin('namelist and scratch')
    open(newunit=u, status='scratch', form='formatted')
    a = 3; b = -4; c = 2.5d0; nm = 'hello'
    write(u, nml=params)
    a = 0; b = 0; c = 0.0d0; nm = ''
    rewind(u)
    read(u, nml=params)
    call ok(a == 3 .and. b == -4 .and. c == 2.5d0 .and. nm == 'hello', 'namelist round trip through a scratch file')
    close(u)
    open(newunit=u, status='scratch', form='formatted')
    write(u, '(a)') ' &params a=10 c=1d2 /'
    rewind(u)
    read(u, nml=params)
    call ok(a == 10 .and. c == 100.0d0 .and. b == -4, 'namelist read leaves other variables alone')
    close(u)
    open(newunit=u, status='scratch', form='formatted')
    write(u, *) 1, 2.5d0, 'text', .true., (3.0d0, 4.0d0)
    rewind(u)
    block
      integer :: i1
      double precision :: d1
      character(len=4) :: t1
      logical :: l1
      complex(kind(1.0d0)) :: z1
      read(u, *) i1, d1, t1, l1, z1
      call ok(i1 == 1 .and. d1 == 2.5d0 .and. t1 == 'text' .and. l1 .and. z1 == (3.0d0, 4.0d0), 'list-directed write and read of mixed types')
    end block
    close(u)
    call finish()
  end subroutine
end program fio
