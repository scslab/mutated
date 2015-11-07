package main

import (
	"math"
	"log"
	"net"
	"time"
	"encoding/binary"
)

var itersPerUsec int64

func workload(n int64) {
	for i := int64(0); i < n; i++ {
		math.Sqrt(float64(1024))
	}
}

type reqPkt struct {
	Nr int32
	Pad int32
	Tag uint64
	Delays[16] uint64
}

func serveRequest(c net.Conn) {
	var req reqPkt
	var resp uint64

	defer c.Close()

	err := binary.Read(c, binary.LittleEndian, &req)
	if err != nil {
		return
	}

	//log.Print(req)

	resp = req.Tag
	workload(int64(req.Delays[0]) * itersPerUsec)
	binary.Write(c, binary.LittleEndian, &resp)
}

func callibrate() {
	const N = 10000000
	start := time.Now()

	workload(N)

	itersPerUsec = N / (time.Since(start).Nanoseconds() / 1000)
	log.Print("iters per usec:", itersPerUsec)
}

func main() {
	callibrate()

	l, err := net.Listen("tcp", ":8080")
	if err != nil {
		log.Fatal("listen error", err)
	}

	for {
		conn, err := l.Accept()
		if err != nil {
			log.Print("accept error", err)
			continue
		}

		go serveRequest(conn)
	}
}
