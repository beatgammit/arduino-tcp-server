package main

import (
	"net"
	"fmt"
	"log"
	"os"
	"bufio"
	"strconv"
	"encoding/json"
	"encoding/binary"
)

var done chan bool
var out chan int

type Message struct {
	Angle int `json:"angle"`
}

type FrameError string
func (e FrameError) Error() string {
	return string(e)
}

func getLength(r *bufio.Reader) (int64, error) {
	if mode, err := r.ReadByte(); err != nil {
		return -1, err
	} else if mode != 0x81 {
		return -1, FrameError(fmt.Sprintf("Invalid mode: '%x'", mode))
	}

	mask, err := r.ReadByte()
	if err != nil {
		return -1, err
	} else if (mask & 0x80) != 0x80 {
		return -1, FrameError(fmt.Sprintf("Invalid mask: '%x'", mask))
	}

	// get rid of the mask bit to make later comparisons simpler
	mask &= 0x7f

	if mask <= 0x7d {
		// the length fits in the mask, so we're done
		return int64(mask), nil
	}

	// the next 2 or 8 bytes contains the length
	// 0x7e for 2 bytes
	// 0x7f for 8 bytes,
	if mask == 0x7e {
		a, err := r.ReadByte()
		if err != nil {
			return -1, err
		}

		b, err := r.ReadByte()
		if err != nil {
			return -1, err
		}

		return int64((a << 8) | b), nil
	} else if mask == 0x7f {
		var length int64

		var b byte
		for i := 8; i >= 0; i-- {
			b, err = r.ReadByte()
			if err != nil {
				return -1, err
			}

			length |= int64(b << byte(8 * i))
		}

		return length, nil
	}

	// can't get here, all cases handled
	return -1, nil
}

func handleRead(c net.Conn) {
	r := bufio.NewReader(c)
	for {
		length, err := getLength(r)
		if err != nil {
			log.Println(err)
			if _, isFrameErr := err.(FrameError); isFrameErr {
				// give up and continue loop
				log.Println(err)
				continue
			} else {
				return
			}
		}

		content := make([]byte, length)
		n, err := r.Read(content)

		l := 0
		if err != nil {
			log.Println(err)
		}

		for ; err == nil && n + l < len(content); l, err = c.Read(content[n:]) {
			n += l
		}

		fmt.Printf("  Reply: %s\n", content)
		fmt.Print("Enter angle: ")
	}
}

func handleWrite(c net.Conn) {
	for angle := range out {
		m := Message{angle}
		if buf, err := json.Marshal(m); err != nil {
			log.Printf("Could not json encode angle: %+v\n", m)
		} else {
			var frame []byte
			mode := byte(0x81)
			mask := byte(0x80)
			if len(buf) <= 125 {
				// can fit in the mask
				frame = []byte{mode, mask | byte(len(buf))}
			} else if len(buf) > 0xffff {
				// needs 8 extra bytes
				frame = make([]byte, 10)
				frame[0] = mode
				frame[1] = mask | 127
				// for now, len is 32-bit
				// TODO: update when 64-bit is supported
				// this should never happen anyway
				binary.PutVarint(frame[2:], int64(len(buf)))
			} else {
				// needs 2 extra bytes
				frame = []byte{
					mode,
					mask | 128,
					byte(len(buf) & 0xff00) >> 8,
					byte(len(buf) & 0xff)}
			}

			c.Write(append(frame, buf...))
		}
	}
}

func handleStdio() {
	r := bufio.NewReader(os.Stdin)

	fmt.Print("Enter angle: ")
	for {
		line, isPrefix, err := r.ReadLine()
		if err != nil {
			panic(err)
		}

		var l []byte
		for isPrefix {
			l, isPrefix, err = r.ReadLine()
			if err != nil {
				panic(err)
			}

			line = append(line, l...)
		}

		angle, err := strconv.Atoi(string(line))
		if err != nil {
			fmt.Printf("\nInvalid angle: %s\n", line)
			fmt.Print("Enter angle: ")
		} else {
			// valid angle, so send it
			out<-angle
		}
	}
}

func main() {
	conn, err := net.Dial("tcp", "192.168.254.177:5000")
	if err != nil {
		panic(err)
	}

	defer conn.Close()

	done = make(chan bool)
	out = make(chan int)

	defer func() {
		close(done)
		close(out)
	}()

	go handleRead(conn)
	go handleWrite(conn)
	go handleStdio()

	<-done
	fmt.Println("All done, ending program")
}
