The microconsole tool is a simple console for a PCs and an embedded systems.
It supports a variety speeds of UART and has a support for simple TTS mechanism.

Supported speeds: 50 bps - 4 Mbps

Default speed is 115200 bps

	Usage:
		ucon <device> <speed> [optional_ttc_file]

	Example
		ucon /dev/ttyUSB0 921600 trigger_to_command.txt


	Basic shortcut keys help:
		Ctrl-X - exit

		Ctrl-A or Ctrl-D is a command key combination.
		Press it and next command letter:
			H/h - print help
			Q/q - exit
			C/c - clear screen
			T/t - enable/disable time stamp at start of each line
			M/m - trigger to command enable/disable
			U/u - increase port speed
			D/d - decrease port speed

	Example:
		Ctrl-D C or Ctrl-A C - clear the screen

