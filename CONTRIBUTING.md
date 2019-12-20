# Contributing Guide

We welcome every sort of contribution, from bug reports to improvements of the code or new features. If you are not yet
familiar with contributions to open source projects, don't worry. It's not that difficult and most important steps are
explained below.

There are mainly two ways of contribution:

1. If you find a bug or have an idea for a good new feature, just add an **issue** in the GitHub web interface.
2. If you already know how to implement a solution, follow the guide to contribute code below using a **pull-request**.
   In case you are not sure about the implementation, raise an issue first to discuss ideas with others.

Please note also our [Code of Conduct](CODE_OF_CONDUCT.md)

## How can I contribute code?

Here is a quick summary how contributions work on GitHub:

1. Fork the repository to your local GitHub account
2. Clone the repository to your local computer using `git clone https://github.com/your-github-username/repository-name.git`
3. Create a new branch where you will later add your changes using `git switch -c branch-name`
4. Update the files to fix the issue or add new features. Please add your copyright notice in a new line if you made significant changes to the file.
5. Use `git add your-changed-files` to add the file contents of the changed files to the so-called staging area (means
   they are prepared to be committed)
6. Run `git commit -m "Short message to describe the changes"` to add the staged files to the respository
7. Upload the changes to the remote repository on GitHub using `git push origin branch-name`
8. Submit a pull request via the GitHub web interface and wait for the response of the maintainers.

### Important remarks:

- Better make several small [atomic commits](https://en.wikipedia.org/wiki/Atomic_commit#Atomic_commit_convention) than
  a single big one
- Use meaningful commit messages and pull request topics. Tell *what* was changed and not that *something* was changed.
  "ADC measurements: Improved performance of temperature sensor reading" is much better than "Updates". You can also
  reference an issue with its numer (e.g. #123) in the commit message
- Use the [Libre Solar coding style](https://libre.solar/docs/coding_style/)

## License

All contributions must be licensed under the [Apache License 2.0](LICENSE) as the rest of the repository.
