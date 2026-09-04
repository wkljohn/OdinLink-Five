# Issue tracker lookup

Use this workflow when a review, commit, branch, or user request references an
issue or pull request. Tracker access is read-only unless the user explicitly
asks for a write.

## GitHub

1. Resolve the repository from `git remote -v`. Prefer the user's fork for
   fork-local work and the named upstream repository for upstream issues.
2. Read an issue with:

   ```sh
   gh issue view NUMBER --repo OWNER/REPO \
     --json number,title,body,state,author,labels,comments,url
   ```

3. Read a pull request with:

   ```sh
   gh pr view NUMBER --repo OWNER/REPO \
     --json number,title,body,state,author,baseRefName,headRefName,commits,files,reviews,comments,url
   ```

4. If authentication is unavailable, use the public GitHub page or API and
   record that comments or private context may be incomplete.

## GitLab

Use `glab issue view NUMBER --repo OWNER/REPO --comments` for issues and
`glab mr view NUMBER --repo OWNER/REPO --comments` for merge requests.

## Review record

Before reviewing code, record the tracker URL, title, requested behavior,
acceptance criteria, and any maintainer clarification that changes the scope.
Quote only the lines needed to support a finding. The lookup is complete when
the fixed code range and its controlling requirements are both unambiguous.
