#
# This test file concludes tests which point out known bugs.
# So all tests will cause failure.
#

assert_equal '0', %q{
  GC.stress = true
  pid = fork {}
  Process.wait pid
  $?.to_i
}, '[ruby-dev:32404]'

assert_normal_exit %q{
  "abcdefghij\xf0".force_encoding("utf-8").reverse.inspect
}, '[ruby-dev:32448]'

assert_equal 'ok', %q{
  class C
    define_method(:foo) do |arg, &block|
      if block then block.call else arg end
    end
  end
  C.new.foo("ng") {"ok"}
}, '[ruby-talk:266422]'

assert_equal 'ok', %q{
  STDERR.reopen(STDOUT)
  class C
    define_method(:foo) do |&block|
      block.call if block
    end
    result = "ng"
    new.foo() {result = "ok"}
    result
  end
}

assert_equal 'ok', %q{
  begin
    if ("\xa1\xa2\xa1\xa3").force_encoding("euc-jp").split(//) ==
      ["\xa1\xa2".force_encoding("euc-jp"), "\xa1\xa3".force_encoding("euc-jp")]
      :ok
    else
      :ng
    end
  rescue
    :ng
  end
}, '[ruby-dev:32452]'
