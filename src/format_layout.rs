use crate::nodes::SourceInfo;

const GRID_WIDTH: usize = 60;

pub struct Row {
    pub source_info: SourceInfo,
    pub key: Option<String>,
    pub val: String,
}

pub fn indent(s: &str) -> String {
    let lines: Vec<String> = s
        .split("\n")
        .map(|line| {
            if line.trim().len() == 0 {
                return String::new();
            }
            return format!("\t{}", line);
        })
        .collect();
    lines.join("\n")
}

pub fn fmt_list(rows: &Vec<Row>) -> String {
    let mut totalwidth = 0;
    let mut maxwidth = 0;
    let mut have_comments = false;
    let mut have_keys = false;
    for row in rows {
        let mut n = row.val.len();
        if let Some(k) = &row.key {
            n += k.len() + 3;
            have_keys = true;
        }
        if row.source_info.trailing_comment.is_some() || row.source_info.comments.is_some() {
            have_comments = true;
        }
        totalwidth += n;
        if n > maxwidth {
            maxwidth = n;
        }
    }

    let mut mode = "default";
    if totalwidth < GRID_WIDTH && !have_comments && !have_keys {
        mode = "oneline";
    } else if maxwidth < 12 && !have_comments && !have_keys {
        mode = "grid";
    }

    let mut s = String::new();
    match mode {
        "oneline" => {
            let items: Vec<String> = rows.iter().map(|x| x.val.clone()).collect();
            s += &fmt_list_oneline(&items);
        }
        "grid" => {
            let items: Vec<String> = rows.iter().map(|x| x.val.clone()).collect();
            s += &fmt_list_grid(&items, maxwidth);
        }
        "default" => {
            s += &fmt_vertical(rows);
        }
        _ => todo!(),
    }

    s
}

fn fmt_vertical(rows: &Vec<Row>) -> String {
    let mut s = String::new();
    s += "{\n";
    for row in rows {
        let mut item = fmt_comments(&row.source_info.comments);
        if let Some(k) = &row.key {
            item += k;
            item += " = ";
        }
        item += &row.val;
        item += ",";
        if let Some(c) = &row.source_info.trailing_comment {
            item += " ";
            item += c;
        }

        s += &indent(&item);
        s += "\n";
    }
    s += "}";
    s
}

fn fmt_list_oneline(items: &Vec<String>) -> String {
    let mut s = String::new();
    s += "{ ";
    for (i, item) in items.iter().enumerate() {
        if i > 0 {
            s += ", ";
        }
        s += &item;
    }
    s += " }";
    s
}

fn fmt_list_grid(items: &Vec<String>, maxwidth: usize) -> String {
    let mut s = String::new();
    let perrow = GRID_WIDTH / (maxwidth + 2);
    s += "{\n";
    for (i, item) in items.iter().enumerate() {
        let padded = format!("{:>maxwidth$}", item);
        if i == 0 {
            s += "\t";
            s += &padded;
            continue;
        }
        if i % perrow == 0 {
            s += ",\n\t";
        } else {
            s += ", ";
        }
        s += &padded
    }
    s += "\n}";
    s
}

pub fn fmt_comments(x: &Option<Vec<String>>) -> String {
    let mut s = String::new();
    if x.is_none() {
        return s;
    }
    for c in x.as_ref().unwrap() {
        if c.starts_with("/*") {
            s += &fmt_multiline_comment(&c);
            s += "\n";
            continue;
        }
        s += c;
        s += "\n";
    }
    s
}

fn fmt_multiline_comment(c: &str) -> String {
    let lines = parse_ml(c);
    if lines.len() == 1 {
        return format!("// {}", &lines[0]);
    }
    lines.join("\n")
}

fn parse_ml(c: &str) -> Vec<String> {
    let lines: Vec<String> = c.split("\n").map(|x| String::from(x)).collect();

    let is_stars_doc = lines
        .iter()
        .skip(1)
        .all(|line| line.trim().starts_with("*"));

    let mut minindent = lines
        .iter()
        .skip(1)
        .map(|line| indent_size(line))
        .min()
        .unwrap_or(0);

    if minindent > 0 && is_stars_doc {
        minindent -= 1;
    }

    lines
        .iter()
        .map(|line| {
            let ind = indent_size(line);
            let eff = if ind > 0 { ind - minindent } else { 0 };
            format!("{}{}", " ".repeat(eff), line.trim())
        })
        .collect()
}

fn indent_size(s: &str) -> usize {
    let mut n = 0;
    for c in s.chars() {
        match c {
            ' ' => n += 1,
            '\t' => n += 4,
            _ => break,
        }
    }
    n
}
