import re

def remove_third_column(input_file, output_file):
    try:
        with open(input_file, 'r') as infile, open(output_file, 'w') as outfile:
            for line in infile:
                parts = line.split()
                if len(parts) >= 3:
                    parts.pop(2)
                    outfile.write(' '.join(parts) + '\n')
                    # outfile.write(line)
    except FileNotFoundError:
        print(f"Error: Input file '{input_file}' not found.")
    except Exception as e:
        print(f"An error occurred: {e}")

# input_file = "lat-spring-data/public1.res"
# output_file = "lat-spring-data/public1_trimed.res"
# input_file = "lat-spring-data/public2.res"
# output_file = "lat-spring-data/public2_trimed.res"

input_file = "out.log"
output_file = "out_trimed.log"
remove_third_column(input_file, output_file)
print(f"Successfully processed '{input_file}' and created '{output_file}'")
