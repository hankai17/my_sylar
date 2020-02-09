package main

import (
	"bufio"
	"bytes"
	"fmt"
	"io"
	"time"
	//"io/ioutil"
	"os/exec"
    "strconv"
    "strings"
)

func demo1() {
	useBufferIo := false
	fmt.Println("Run command `socket_test`: ")
	cmd0 := exec.Command("./socket_test")
	stdout0, err := cmd0.StdoutPipe() //stdout0是io.ReadCloser类型 这是个接口类型并扩展了接口类型io.Reader so就可以调用Read方法获取输出了
	if err != nil {
		fmt.Printf("Error: Can not obtain the stdout pipe for command No.0: %s\n", err)
		return
	}

	if err := cmd0.Start(); err != nil {
		fmt.Printf("Error: The command No.0 can not be startup: %s\n", err)
		return
	}

	if !useBufferIo { //思路是先从管道里依次读到切片里 然后依次写到缓冲区里 
		var outputBuf0 bytes.Buffer //定义一个缓冲区
		for {
			tempOutput := make([]byte, 5)
			n, err := stdout0.Read(tempOutput) //调用Read方法
			if err != nil {
				if err == io.EOF {
					break
				} else {
					fmt.Printf("Error: Can not read data from the pipe: %s\n", err)
					return
				}
			}
			if n > 0 {
				outputBuf0.Write(tempOutput[:n])
			}
		}
		fmt.Printf("%s\n", outputBuf0.String())
	} else { //直接读到一个带缓冲器的容器中(带缓冲器的读取器bufio 默认4096字节长度)
		outputBuf0 := bufio.NewReader(stdout0) //stdout0的值也是io.Reader类型 so可以直接作为bufio的参数
		output0, _, err := outputBuf0.ReadLine() //从读取器中读一行 //第二个参数为false 意为当前行未读完
		if err != nil {
			fmt.Printf("Error: Can not read data from the pipe: %s\n", err)
			return
		}
		fmt.Printf("%s\n", string(output0))
	}
}

//func md5_check(file string, ch chan string) {
func md5_check(file string ) (result string){
	cmd0 := exec.Command("md5sum", file)
	stdout0, err := cmd0.StdoutPipe()
	if err != nil {
		fmt.Printf("Error: Can not obtain the stdout pipe for command No.0: %s\n", err)
		return
	}

	if err := cmd0.Start(); err != nil {
		fmt.Printf("Error: The command No.0 can not be startup: %s\n", err)
		return
	}
	var outputBuf0 bytes.Buffer //定义一个缓冲区
	for {
		tempOutput := make([]byte, 5)
		n, err := stdout0.Read(tempOutput) //调用Read方法
		if err != nil {
			if err == io.EOF {
				break
			} else {
				fmt.Printf("Error: Can not read data from the pipe: %s\n", err)
				return
			}
		}
		if n > 0 {
			outputBuf0.Write(tempOutput[:n])
		}
	}
	//fmt.Printf("%s\n", outputBuf0.String())
	return outputBuf0.String()
}

func test() {
	demo1()
	time.Sleep(time.Second * 1)
    /*
    chs := make([]chan string, 10)
    for i := 0; i < 10; i++ {
        file = "1.dat" + string(i)
        chs[i]  = make(chan(string))
        go md5_check(file, chs[i])
    }
    */

    for i := 0; i < 10; i++ {
        file := "1.dat" + strconv.Itoa(i)
        tmp := md5_check(file)
        //fmt.Println(tmp) // 471580ab3ee06bc58cd6fdc68d7ca763
        if (!strings.Contains(tmp, "471580ab3ee06bc58cd6fdc68d7ca763")) {
            fmt.Println("not ok!")
        } else {
            fmt.Println(tmp)
        }
    }
}

func main() {
  for i := 0; i < 1000; i++ {
    test()
  }
}
