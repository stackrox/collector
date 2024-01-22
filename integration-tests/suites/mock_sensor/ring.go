package mock_sensor

type RingChan[T any] struct {
	in  chan T
	out chan T
}

// NewRingChan initializes a RingChan of a given size.
// The input and output channels will have the same
// capacity as defined by the size parameter
//
// Note: for every RingChan, a new goroutine is started
func NewRingChan[T any](size int) RingChan[T] {
	r := RingChan[T]{
		in:  make(chan T, size),
		out: make(chan T, size),
	}

	go r.run()

	return r
}

// Write a value to the input channel
func (r *RingChan[T]) Write(item T) {
	r.in <- item
}

// Read a value from the output channel
func (r *RingChan[T]) Read() T {
	return <-r.out
}

// Stream returns the output channel
func (r *RingChan[T]) Stream() chan T {
	return r.out
}

// Stop closes the channels. The side effect of this is that
// the gorountine will stop.
func (r *RingChan[T]) Stop() {
	close(r.in)
	close(r.out)
}

// run handles the shuffling of data between the input and output channels.
// if the output channel is full (and no one is reading from it)
// this function will pop a value from the front, and queue the new
// value to the back. Based on the built-in channel buffer size, this acts
// as a ring buffer.
func (r *RingChan[T]) run() {
	for v := range r.in {
		select {
		case r.out <- v:
			// if out is writable, write it straight to the channel
		default:
			// otherwise, pop the oldest item from the out channel,
			// and enqueue the next item
			<-r.out
			r.out <- v
		}
	}
}
