#
# User makefile.
# Edit this file to change compiler options and related stuff.
#

# Programmer interface configuration, see http://dev.bertos.org/wiki/ProgrammerInterface for help
#TinyAPRS_PROGRAMMER_TYPE = usbasp
#TinyAPRS_PROGRAMMER_PORT = asp

TinyAPRS_PROGRAMMER_TYPE = arduino

# USB = PL2303
TinyAPRS_PROGRAMMER_PORT = /dev/cu.usbserial
TinyAPRS_PROGRAMMER_BAUD = 57600

# USB = CP2102
#TinyAPRS_PROGRAMMER_PORT = /dev/cu.SLAB_USBtoUART
#TinyAPRS_PROGRAMMER_BAUD = 57600

# USB = FT232
#TinyAPRS_PROGRAMMER_PORT = /dev/cu.usbserial-AH02KRAG
#TinyAPRS_PROGRAMMER_PORT = /dev/cu.usbserial-AH02KRAF
#TinyAPRS_PROGRAMMER_BAUD = 115200

# Files included by the user.
TinyAPRS_USER_CSRC = \
	$(TinyAPRS_SRC_PATH)/main.c \
	$(TinyAPRS_SRC_PATH)/hw/hw_afsk.c \
	$(TinyAPRS_SRC_PATH)/lcd/hw_lcd_4884.c \
	$(TinyAPRS_SRC_PATH)/console.c \
	$(TinyAPRS_SRC_PATH)/settings.c \
	$(TinyAPRS_SRC_PATH)/net/kiss.c \
	$(TinyAPRS_SRC_PATH)/utils.c \
	$(TinyAPRS_SRC_PATH)/beacon.c \
	$(TinyAPRS_SRC_PATH)/gps.c \
	$(TinyAPRS_SRC_PATH)/reader.c \
	$(TinyAPRS_SRC_PATH)/digi.c \
	#$(TinyAPRS_SRC_PATH)/hw/hw_softser.c \
	#$(TinyAPRS_SRC_PATH)/radio.c \
	#

# Files included by the user.
TinyAPRS_USER_PCSRC = \
	#

# Files included by the user.
TinyAPRS_USER_CPPASRC = \
	#

# Files included by the user.
TinyAPRS_USER_CXXSRC = \
	#

# Files included by the user.
TinyAPRS_USER_ASRC = \
	#

# Flags included by the user.
TinyAPRS_USER_LDFLAGS = \
	#

# Flags included by the user.
TinyAPRS_USER_CPPAFLAGS = \
	#

# Flags included by the user.
TinyAPRS_USER_CPPFLAGS = \
	-fno-strict-aliasing \
	-fwrapv \
	#

# Print binary size, make sure avr-size is in the PATH env
AVRSIZE=avr-size
print_size:
	$(AVRSIZE) --format=avr --mcu=$(TinyAPRS_MCU) $(OUTDIR)/$(TRG).elf

# Just flash the image, make sure avrdude is in the path
flash_image:
	$(AVRDUDE) -p $(TinyAPRS_MCU) -c $(TinyAPRS_PROGRAMMER_TYPE) -P $(TinyAPRS_PROGRAMMER_PORT) -b $(TinyAPRS_PROGRAMMER_BAUD) -F -U flash:w:$(OUTDIR)/$(TRG).hex:i

# Build and flash to the target
flash: flash_$(TRG)
	
