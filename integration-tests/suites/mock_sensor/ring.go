package mock_sensor

type RingChan[T any] struct {
	in  chan T
	out chan T
}

func NewRingChan[T any](size int) RingChan[T] {
	r := RingChan[T]{
		in:  make(chan T, size),
		out: make(chan T, size),
	}

	go r.run()

	return r
}

func (r *RingChan[T]) Write(item T) {
	r.in <- item
}

func (r *RingChan[T]) Read() T {
	return <-r.out
}

func (r *RingChan[T]) Stream() chan T {
	return r.out
}

func (r *RingChan[T]) Stop() {
	close(r.in)
	close(r.out)
}

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
