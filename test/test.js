var mouse = require('../')

var m1 = mouse()

// m1.on('move', function (x, y) {
//   console.log('move_1', x, y)
// })


m1.on('left-down', function (x, y) {
    console.log('left-down', x, y)
})


m1.on('left-up', function (x, y) {
    console.log('left-up', x, y)
})


m1.on('left-drag', function (x, y) {
    console.log('left-drag', x, y)
})