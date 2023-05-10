"""
Iterate on commits of the asyncio Git repository using the Dulwich module.
"""

import os

import dulwich.repo

from sneksit import do_bench
import functools

def iter_all_commits(repo):
    # iterate on all changes on the Git repository
    for entry in repo.get_walker(head):
        pass


if __name__ == "__main__":
    repo_path = os.path.join(os.path.dirname(__file__), 'data', 'asyncio.git')

    repo = dulwich.repo.Repo(repo_path)
    head = repo.head()
    func = functools.partial(iter_all_commits, repo)
    do_bench('dulwich_log', func, 100)
    repo.close()
