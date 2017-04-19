# This ruby script generated the sine wave tables in sine.h

PI = 3.14159265
TWOPI = PI * 2

def generate_sine_pwm(name, phase_offset, direction, count, max)
  print "const unsigned short #{name}[] = { "
  count.times do |n|
    out = (Math.sin((n.to_f / count * TWOPI) + phase_offset * TWOPI / 3) * max).round * direction
    if out > 0
      print out
    else
      print 0
    end
    print ", " unless count == n + 1
  end
  print " };\n"
end

def generate_tccr3a(count, max)
  print "const unsigned char sine_tccr3a[] = { "
  count.times do |n|
    out_a = (Math.sin((n.to_f / count * TWOPI) + 0 * TWOPI / 3) * max).round
    out_b = (Math.sin((n.to_f / count * TWOPI) + 1 * TWOPI / 3) * max).round
    out_c = (Math.sin((n.to_f / count * TWOPI) + 2 * TWOPI / 3) * max).round
    out = 0
    out |= 32  if out_a > 0
    out |= 8   if out_a < 0
    out |= 128 if out_b > 0
    print out
    print ", " unless count == n + 1
  end
  print " };\n"
end

def generate_tccr4a(count, max)
  print "const unsigned char sine_tccr4a[] = { "
  count.times do |n|
    out_a = (Math.sin((n.to_f / count * TWOPI) + 0 * TWOPI / 3) * max).round
    out_b = (Math.sin((n.to_f / count * TWOPI) + 1 * TWOPI / 3) * max).round
    out_c = (Math.sin((n.to_f / count * TWOPI) + 2 * TWOPI / 3) * max).round
    out = 0
    out |= 128 if out_b < 0
    out |= 32  if out_c > 0
    out |= 8   if out_c < 0
    print out
    print ", " unless count == n + 1
  end
  print " };\n"
end

COUNT = 256
generate_sine_pwm('sine_ocr3b', 0,  1, COUNT, 1023)
generate_sine_pwm('sine_ocr3c', 0, -1, COUNT, 1023)

generate_sine_pwm('sine_ocr3a', 1,  1, COUNT, 1023)
generate_sine_pwm('sine_ocr4a', 1, -1, COUNT, 1023)

generate_sine_pwm('sine_ocr4b', 2,  1, COUNT, 1023)
generate_sine_pwm('sine_ocr4c', 2, -1, COUNT, 1023)

generate_tccr3a(COUNT, 1023)
generate_tccr4a(COUNT, 1023)
