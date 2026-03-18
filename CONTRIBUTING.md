# Scope of project

This project does NOT aim at decompiling and **MATCHING** the binary with Minecraft: Wii-U Edition, rather, we want to reach feature parity via decompilation.

To clarify this vague terminology, the goal is obtaining the same functionality by studying the modifications done by 4J in later versions of the game, we use the Wii-U Edition specifically since this contains function symbols which heavily help figuring out modifications.

# Scope of PRs

Every PR should be backed up by a Title Update change and should document the disassembly and decompilation, they're also only allowed to be within 5 Title Updates from the current project parity, as to not overlap with other Title Update changes.

# Guidelines

- **No AI generated code!** We do not want LLM infested code in this codebase! Reverse engineering requires you to understand core parts of the code being decompiled to create a faithful representation, please do not use AI to create code or analyze code (unless it's STL/Boost boilerplate).
- **No Code Liberties for non platform specific code!** It is utterly disallowed to take your own code liberties for changes, as in, you cannot make up implementations or assume their function, all your modifications have to be backed up by disassembly. Platform specific code is an exception to this rule.
- **Must follow the commit style to keep track of changes!** We want to keep track of every feature added and modified by Title Updates so other projects can depend on our changes for their forks, not following convention would make this much harder for other people and this project too!
- **We do not need Quality Of Life changes.** Please keep this repository as vanilla as possible, adding new features or fixing other existing ones without being backed up by a Title Update change does not adhere within the project scope, as mentioned before, please don't take code liberties.
- **Keep PRs and commits individual to their Title Update changes!** Do not add multiple Title Update changes in a singular commit or PR, if it's needed, create another branch in your fork and then PR your specific Feature ID changes to the main repository.

## Credits
- [Minecraft Console's Contribution Guide](https://github.com/smartcmd/MinecraftConsoles/blob/main/CONTRIBUTING.md)