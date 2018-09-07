We follow the standard [GitHub Flow](https://guides.github.com/introduction/flow/) process. Contributions to the IGSIO project are welcome through GitHub pull requests.

See below information about development process and coding rules.

Issue tracking
--------------

- Commit one fix/enhancement at a time (and not multiple independent developments in one single commit) - whenever it's possible without significant extra effort (if you have a choice, commit your fix before start fixing a new problem).
- When changes committed related to a bug (partial fix, etc.) then add a reference to the ticket id to the commit log in the format: re #123 (this will automatically link the changeset to the ticket)

Committing code changes
-----------------------
- Build project and make sure that there are no build errors
- Commit comments: describe what did you change and why (e.g., Added MyClass:MyObject method to allow doing something), if the modification is related to a ticket (which should be usually the case) then include the ticket id in the comments (for example: re #123: Changed something somewhere because of something...; see the Issue tracking section for details)

Coding conventions
------------------

- In case of child class of a VTK class, use the VTK coding convention.
- Use Windows-style end-of-line characters (CR/LF)
- Use 2 spaces to indent lines (do not use tabs)
- Always put curly brackets in new line, and add a new line after it as well. Use curly brackets even if they enclose only one statement (it can cause errors if the developer does not add the brackets when adding another statement in the block).
- If any method returns failure then the method shall log the cause of the failure using vtkErrorMacro().
- Member variable names: use VTK conventions for VTK classes, for other classes use m_ as a prefix to member variable names (e.g., m_SampleVariable)
