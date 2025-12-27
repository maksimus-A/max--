import sys
import string

def index_to_name(index):
    """Convert 0-based index to alphabetic name: a, b, ..., z, aa, ab, ..."""
    letters = string.ascii_lowercase
    name = ""
    while True:
        index, rem = divmod(index, 26)
        name = letters[rem] + name
        if index == 0:
            break
        index -= 1
    return name

def main():
    if len(sys.argv) != 3:
        print("Usage: python gen_ints.py <count> <output_file.in>")
        sys.exit(1)

    count = int(sys.argv[1])
    output_file = sys.argv[2]

    if not output_file.endswith(".in"):
        print("Error: output file must have .in extension")
        sys.exit(1)

    with open(output_file, "w") as f:
        for i in range(count):
            name = index_to_name(i)
            f.write(f"int {name} = 1;\n")

if __name__ == "__main__":
    main()
