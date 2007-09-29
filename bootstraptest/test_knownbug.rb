#
# This test file concludes tests which point out known bugs.
# So all tests will cause failure.
#

assert_finish 1, %q{
  r, w = IO.pipe
  t1 = Thread.new { r.sysread(1) }
  t2 = Thread.new { r.sysread(1) }
  sleep 0.1
  w.write "a"
  sleep 0.1
  w.write "a"
}, '[ruby-dev:31866]'

assert_equal 'ok', %q{
  class Module
    def define_method2(name, &block)
      define_method(name, &block)
    end
  end
  class C
    define_method2(:m) {|x, y| :fail }
  end
  begin
    C.new.m([1,2])
  rescue ArgumentError
    :ok
  end
}

assert_normal_exit %q{
  Marshal.load("\004\b\173\006\"\006k\"\006v", lambda {|v| })
}
