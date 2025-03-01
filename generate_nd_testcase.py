"""
生成多维数组的测试用例
测试用例文件在 testcases/nd_array 目录下
测试命令
autotest -t testcases/ -koopa -s nd_array /root/compiler
autotest -t testcases/ -riscv -s nd_array /root/compiler
"""

import dataclasses
import pathlib


script_path = pathlib.Path(__file__).resolve()
output_directory = script_path.parent / "testcases" / "nd_array"


@dataclasses.dataclass
class NDArrayTestcase:
    dimensions: list[int]
    value_text: str
    expected: list


testcases = [
    NDArrayTestcase(
        dimensions=[2, 3],
        value_text="{1, 2}",
        expected=[1, 2, 0, 0, 0, 0,],
    ),
    NDArrayTestcase(
        dimensions=[2, 3],
        value_text="{1, 2, 3, 4, 5, 6}",
        expected=[1, 2, 3, 4, 5, 6],
    ),
    NDArrayTestcase(
        dimensions=[2, 2, 2],
        value_text="{1, 2}",
        expected=[1, 2, 0, 0, 0, 0, 0, 0],
    ),
    NDArrayTestcase(
        dimensions=[2, 2, 2],
        value_text="{1, 2, 3, 4, 5, 6, 7, 8}",
        expected=[1, 2, 3, 4, 5, 6, 7, 8],
    ),
    NDArrayTestcase(
        dimensions=[2, 3, 4],
        value_text="{1, 2, 3, 4, {5}, {6}, {7, 8}}",
        expected=[1, 2, 3, 4, 5, 0, 0, 0, 6, 0, 0, 0, 7, 8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0],
    ),
    NDArrayTestcase(
        dimensions=[2, 3, 4],
        value_text="{1, 2, 3, 4, {5}}",
        expected=[1, 2, 3, 4, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0],
    ),
    NDArrayTestcase(
        dimensions=[2, 3, 4],
        value_text="{1, 2, 3, 4, 1, 2, 3, 4, 1, 2, 3, 4, {5}}",
        expected=[1, 2, 3, 4, 1, 2, 3, 4, 1, 2, 3, 4, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0],
    ),
    NDArrayTestcase(
        dimensions=[2, 3, 4],
        value_text="{1, 2, 3, 4, 1, 2, 3, 4, 1, 2, 3, 4, {5, 6, 7, 8, {9}}}",
        expected=[1, 2, 3, 4, 1, 2, 3, 4, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 0, 0, 0, 0, 0, 0],
    ),
    NDArrayTestcase(
        dimensions=[2, 3, 4],
        value_text="{1, 2, 3, 4, 1, 2, 3, 4, 1, 2, 3, 4, {{9}, 5, 6, 7, 8}}",
        expected=[1, 2, 3, 4, 1, 2, 3, 4, 1, 2, 3, 4, 9, 0, 0, 0, 5, 6, 7, 8, 0, 0, 0, 0],
    ),
    NDArrayTestcase(
        dimensions=[2, 3, 4],
        value_text="{1, 2, 3, 4, 1, 2, 3, 4, 1, 2, 3, 4, {{9}, 5, 6, 7, 8, {9}}}",
        expected=[1, 2, 3, 4, 1, 2, 3, 4, 1, 2, 3, 4, 9, 0, 0, 0, 5, 6, 7, 8, 9, 0, 0, 0],
    ),
    NDArrayTestcase(
        dimensions=[2, 3, 4],
        value_text="{{}, 2, 3, 4, 5}",
        expected=[0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 3, 4, 5, 0, 0, 0, 0, 0, 0, 0, 0],
    ),
    NDArrayTestcase(
        dimensions=[2, 3, 4],
        value_text="{{1}, 2, 3, 4, 5}",
        expected=[1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 3, 4, 5, 0, 0, 0, 0, 0, 0, 0, 0],
    ),
    NDArrayTestcase(
        dimensions=[2, 3, 4],
        value_text="{{9, 8}, 2, 3, 4, 5}",
        expected=[9, 8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 3, 4, 5, 0, 0, 0, 0, 0, 0, 0, 0],
    ),
    NDArrayTestcase(
        dimensions=[2, 3, 4],
        value_text="{{9, 8, 7}, 2, 3, 4, 5}",
        expected=[9, 8, 7, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 3, 4, 5, 0, 0, 0, 0, 0, 0, 0, 0],
    ),
    NDArrayTestcase(
        dimensions=[2, 3, 4],
        value_text="{{9, 8, 7, 6, {}}, 2, 3, 4, 5}",
        expected=[9, 8, 7, 6, 0, 0, 0, 0, 0, 0, 0, 0, 2, 3, 4, 5, 0, 0, 0, 0, 0, 0, 0, 0],
    ),
    NDArrayTestcase(
        dimensions=[2, 3, 4],
        value_text="{{9, 8, 7, 6, {5}}, 2, 3, 4, 5}",
        expected=[9, 8, 7, 6, 5, 0, 0, 0, 0, 0, 0, 0, 2, 3, 4, 5, 0, 0, 0, 0, 0, 0, 0, 0],
    ),
    NDArrayTestcase(
        dimensions=[2, 3, 4],
        value_text="{{9, 8, 7, 6, {5, 4, 3, 2}}, 2, 3, 4, 5}",
        expected=[9, 8, 7, 6, 5, 4, 3, 2, 0, 0, 0, 0, 2, 3, 4, 5, 0, 0, 0, 0, 0, 0, 0, 0],
    ),
]


template = """{description}
int arr{dimensions_text} = {value_text};

int main() {{
    return arr{access_text};
}}
"""


output_directory.mkdir(parents=True, exist_ok=True)
for i, testcase in enumerate(testcases):
    description = f"// Testcase {i + 1}"
    dimensions_text = "".join(f"[{d}]" for d in testcase.dimensions)

    # Generate the access patterns for each element
    total_elements = 1
    for d in testcase.dimensions:
        total_elements *= d

    for elem_idx in range(total_elements):
        # Calculate the indices for each dimension
        indices = []
        remaining = elem_idx
        for d in reversed(testcase.dimensions):
            indices.insert(0, remaining % d)
            remaining //= d

        # Create the access text (e.g., [0][1])
        access_text = "".join(f"[{idx}]" for idx in indices)

        # Create the source file
        source_code = template.format(
            description=description,
            dimensions_text=dimensions_text,
            value_text=testcase.value_text,
            access_text=access_text
        )

        # Write to file
        expected_value = testcase.expected[elem_idx]
        source_file = output_directory / f"nd_array_{i}_{elem_idx}.c"
        with open(source_file, "w") as f:
            f.write(source_code)

        # Write expected output
        expected_file = output_directory / f"nd_array_{i}_{elem_idx}.out"
        with open(expected_file, "w") as f:
            f.write(str(expected_value))
