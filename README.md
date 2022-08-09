This is a simple benchmark to test if there is any performance benefit for using 1D over 2D textures.

## Results
In my computer, I have found that there is a very small differerence. The following table measures seconds:

| GPU - OS - driver version | 1D | 2D |  Ratio  |
|-----------|---------|---------|---------|
| GTX 750Ti - Windows 10 - 496.13 | 12.2418 | 12.3267 |  +0.69% |
|           |         |         |   |

If you want to share your measurements, feel free to make a pull request.

## How to run the benchmark

Download this repo.

Compile as any normal CMake project.

```
cd tex1d_benchmark
mkdir build
cd build
cmake ..
make
```

If you are using Windows, and don't want to compile yourself, you can find an .exe in [releases](https://github.com/tuket/opengl_tex1d_benchmark/releases).

Run the generated `tex1d_benchmark` executable.

It will print something like this to the console (stdout):

```
1D: 12.2418
2D: 12.3267
```
