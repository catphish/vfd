# This ruby script generated the sine wave tables in sine.h

PI = 3.14159265
TWOPI = PI * 2

def generate_sine_pwm(name, phase_offset, count, max)
  print "const uint16_t #{name}[] = { "
  count.times do |n|
    out = (Math.sin((n.to_f / count * TWOPI) + phase_offset * TWOPI / 3) * max).round
    print out.abs
    print ", " unless count == n + 1
  end
  print " };\n"
end

def generate_sine_direction(name, phase_offset, count, max)
  print "const uint8_t #{name}[] = { "
  count.times do |n|
    out = (Math.sin((n.to_f / count * TWOPI) + phase_offset * TWOPI / 3) * max).round
    if out >= 0
      print 1
    else
      print 0
    end
    print ", " unless count == n + 1
  end
  print " };\n"
end

COUNT = 256
generate_sine_pwm(      'sine_a',           0, COUNT, 511)
generate_sine_direction('sine_a_direction', 0, COUNT, 511)
generate_sine_pwm(      'sine_b',           1, COUNT, 511)
generate_sine_direction('sine_b_direction', 1, COUNT, 511)
generate_sine_pwm(      'sine_c',           2, COUNT, 511)
generate_sine_direction('sine_c_direction', 2, COUNT, 511)
