import os
import re

def remove_klog_calls(content):
    output = []
    i = 0
    length = len(content)
    
    while i < length:
        # Check for 'klog('
        if content[i:].startswith('klog('):
            # We found a klog call. Now find the end of it.
            # We need to balance parentheses.
            paren_depth = 0
            j = i + 4 # Skip 'klog'
            
            # Scan forward
            while j < length:
                char = content[j]
                if char == '(': 
                    paren_depth += 1
                elif char == ')':
                    paren_depth -= 1
                    if paren_depth == 0:
                        # Found the closing parenthesis
                        
                        # Now checks for the semicolon after the call
                        k = j + 1
                        while k < length and content[k].isspace():
                            if content[k] == '\n':
                                # Don't consume newline yet if we want to preserve line structure? 
                                # Actually, usually we want to consume the semicolon.
                                pass 
                            k += 1
                        
                        if k < length and content[k] == ';':
                            # Consume semicolon
                            j = k
                        
                        # Move main index i to j + 1 (after the semicolon/closing paren)
                        i = j + 1
                        
                        # Consume any immediate following newline to avoid leaving blank lines
                        if i < length and content[i] == '\n':
                            i += 1
                        elif i < length and content[i] == '\r' and i+1 < length and content[i+1] == '\n':
                            i += 2
                            
                        break
                j += 1
            else:
                # If loop finishes without break, we didn't find balanced parens?
                # Just copy the 'klog' and continue (fallback)
                output.append(content[i])
                i += 1
        else:
            output.append(content[i])
            i += 1
            
    return "".join(output)

def process_directory(directory):
    for root, dirs, files in os.walk(directory):
        for file in files:
            if file.endswith('.c') or file.endswith('.h'):
                filepath = os.path.join(root, file)
                print(f"Processing {filepath}...")
                
                with open(filepath, 'r') as f:
                    content = f.read()
                
                new_content = remove_klog_calls(content)
                
                if content != new_content:
                    print(f"  Modified {filepath}")
                    with open(filepath, 'w') as f:
                        f.write(new_content)

if __name__ == "__main__":
    process_directory("kernel")
