use passes::{BuildState, ComponentName, DirLocationPass, SystemState, Transition};
use std::collections::HashMap;

pub struct Repos {
    dirs: HashMap<ComponentName, String>,
}

impl DirLocationPass for Repos {
    fn dir_location(&self, c: &ComponentName) -> &String {
        // unwrap valid as a location is added for each component
        self.dirs.get(&c).unwrap()
    }
}

impl Transition for Repos {
    fn transition(s: &SystemState, b: &mut dyn BuildState) -> Result<Box<Self>, String> {
        let spec = s.get_spec();
        let mut repos: Vec<&String> = spec
            .names()
            .iter()
            .filter_map(|cn| spec.component_named(cn).repo.as_ref())
            .collect();
        repos.sort();
        repos.dedup();
        let comp_locations = spec.names().iter().map(|c| {
            (
                c.clone(),
                &spec.component_named(&c).source,
                &spec.component_named(&c).repo,
            )
        });

        fn repo_dir_create(r: &String) -> String {
            r.replace("/", "-").replace(":", "-")
        }

        // Download any repos that aren't already present.
        for r in repos {
            // Lets parse the repo into the URL to pass to `git`, and into
            // the path in the source repo.
            let repo_split: Vec<&str> = r.split(':').collect();
            if repo_split.len() != 2 {
                return Err(format!(
                    r#"Repo specification "{}" must have the form <repo service>:<repo location> (e.g. "github.com:gparmer/hello_world").\n"#,
                    r
                ));
            }
            if repo_split[0] != "github" {
                return Err(format!(
                    r#"Repo specification provider "{}" is not supported (supported: "github").\n"#,
                    repo_split[0]
                ));
            }

            let repo_url = format!("git@github.com:{}", repo_split[1]);

            b.repo_download(&repo_url, &repo_dir_create(&r), &s)?;
        }

        // Add the paths of the components (in external repos, and in
        // the standard source) to the DirLocationpass-queryable
        // hashmap.
        let mut dirs = HashMap::new();
        for (name, source, repo) in comp_locations {
            let loc = source
                .as_str()
                .split(&['.', '/'][..])
                .collect::<Vec<_>>()
                .join("/");
            dirs.insert(
                name,
                match repo {
                    Some(r) => format!("repos/{}/{}", repo_dir_create(&r.clone()), &loc),
                    None => String::from("implementation/") + &loc,
                },
            );
        }

        Ok(Box::new(Repos { dirs }))
    }
}
