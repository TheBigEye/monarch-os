set architecture i386
set pagination off
set confirm off

# Start QEMU with -S -gdb tcp::1234 and connect with:
#   gdb .build/kernel-debug-gdb.elf -x scripts/gdb-floppy.gdb

target remote :1234

break floppy_read_sector
break floppy_format_track
break floppy_seek
break read_result

commands 1
  silent
  printf "\n[floppy] read_sector entry\n"
  printf "lba=%u buffer=%p\n", $lba, $buffer
  continue
end

commands 2
  silent
  printf "\n[floppy] format_track entry\n"
  printf "cylinder=%u head=%u\n", $cylinder, $head
  continue
end

commands 3
  silent
  printf "\n[floppy] seek entry\n"
  printf "cylinder=%u\n", $cylinder
  continue
end

commands 4
  silent
  printf "\n[floppy] result phase\n"
  printf "count=%u\n", $count
  x/8ub result
  continue
end

# Useful manual inspections at a breakpoint:
#   p/x command
#   p/x self->base
#   p/x self->irq_seen
#   p/x self->last_status
#   x/16ub dma_buffer
